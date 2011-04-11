#include <erl_nif.h>
#include <cstdio>

#include "exc.hpp"
#include "async_queue.h"
#include "snappy.h"
#include "snappy-sinksource.h"

static const size_t ALLOC_SIZE = 2048;

class Sink: public snappy::Sink {
public:

    Sink(): used(0)
    {
        if (!enif_alloc_binary(0, &bin)) {
            enif_release_binary(&bin);
            THROW_EXC("enif_alloc_binary() failed");
        }
    }

    void Append(const char *data, size_t n)
    {
        if (data != reinterpret_cast<const char*>(bin.data + used)) {
            memcpy(bin.data + used, data, n);
        }

        used += n;
    }

    char* GetAppendBuffer(size_t len, char*)
    {
        size_t allocated = bin.size;

        if ((used + len) > allocated) {
            size_t sz = len > ALLOC_SIZE ? len + ALLOC_SIZE - (len % ALLOC_SIZE) : ALLOC_SIZE;

            if (!enif_realloc_binary(&bin, allocated + sz)) {
                enif_release_binary(&bin);
                THROW_EXC("enif_realloc_binary() failed");
            }
        }

        return reinterpret_cast<char*>(bin.data + used);
    }

    ErlNifBinary& GetBinary()
    {
        size_t allocated = bin.size;

        if (allocated > used) {
            if (!enif_realloc_binary(&bin, used)) {
                enif_release_binary(&bin);
                THROW_EXC("enif_realloc_binary() failed");
            }
        }

        return bin;
    }

private:

    ErlNifBinary  bin;
    size_t        used;
};

extern "C" {

typedef struct {
    async_queue_t    *queue;
    ErlNifThreadOpts *topts;
    ErlNifTid         tid;
} ctx_t;

typedef enum {
    UNKNOWN,
    COMPRESS,
    DECOMPRESS,
    SHUTDOWN
} task_type_t;

typedef struct {
    task_type_t   type;
    ErlNifEnv    *env;
    ErlNifPid     pid;
    ERL_NIF_TERM  ref;
    ErlNifBinary  data;
} task_t;

static ErlNifResourceType *res_type;
static ERL_NIF_TERM        atom_ok;
static ERL_NIF_TERM        atom_error;

static void cleanup_task(task_t **task);
static task_t* init_task(task_type_t type, ERL_NIF_TERM ref, ErlNifPid pid,
    ERL_NIF_TERM orig_term);
static task_t* init_empty_task(task_type_t type);
static ctx_t* init_ctx();
static ERL_NIF_TERM compress(task_t *task);
static ERL_NIF_TERM decompress(task_t *task);
static void* worker(void *arg);

void resource_dtor(ErlNifEnv *env, void *obj);
int load(ErlNifEnv *env, void **priv, ERL_NIF_TERM load_info);

void
cleanup_task(task_t **task)
{
    if ((*task)->env != NULL) {
        enif_free_env((*task)->env);
    }

    enif_free(*task);
    *task = NULL;
}

task_t*
init_empty_task(task_type_t type)
{
    task_t *task;

    task = static_cast<task_t*>(enif_alloc(sizeof(*task)));

    if (task == NULL) {
        goto done;
    }

    memset(task, 0, sizeof(*task));
    task->type = type;

done:
    return task;
}

task_t*
init_task(task_type_t type, ERL_NIF_TERM ref, ErlNifPid pid,
    ERL_NIF_TERM orig_term)
{
    ERL_NIF_TERM  term;
    task_t       *task;

    task = init_empty_task(type);

    task->pid = pid;
    task->env = enif_alloc_env();

    if (task->env == NULL) {
        cleanup_task(&task);
        goto done;
    }

    term = enif_make_copy(task->env, orig_term);

    if (!enif_inspect_binary(task->env, term, &task->data)) {
        cleanup_task(&task);
        goto done;
    }

    task->ref = enif_make_copy(task->env, ref);

done:
    return task;
}

ERL_NIF_TERM
compress(task_t *task)
{
    ERL_NIF_TERM result;

    snappy::ByteArraySource source(reinterpret_cast<const char*>(task->data.data), task->data.size);

    Sink sink;

    try {
        snappy::Compress(&source, &sink);
        result = enif_make_tuple3(task->env, atom_ok, task->ref,
            enif_make_binary(task->env, &sink.GetBinary()));
    } catch (std::exception &e) {
        result = enif_make_tuple2(task->env, atom_error,
            enif_make_string(task->env, "Failed to compress",
                ERL_NIF_LATIN1));
    }

    return result;
}

ERL_NIF_TERM
decompress(task_t *task)
{
    size_t len;
    bool   status;

    ERL_NIF_TERM result;
    ErlNifBinary bin;

    len = -1;
    status = snappy::GetUncompressedLength(
        reinterpret_cast<const char *>(task->data.data),
        task->data.size, &len);

    if (!status) {
        result = enif_make_tuple2(task->env, atom_error,
            enif_make_string(task->env, "Data is not compressed",
                ERL_NIF_LATIN1));
        goto done;
    }

    if (!enif_alloc_binary(len, &bin)) {
        result = enif_make_tuple2(task->env, atom_error,
            enif_make_string(task->env, "Couldn't allocate memory",
                ERL_NIF_LATIN1));
        goto done;
    }

    status = snappy::RawUncompress(
        reinterpret_cast<const char*>(task->data.data),
        task->data.size, reinterpret_cast<char*>(bin.data));

    if (!status) {
        result = enif_make_tuple2(task->env, atom_error,
            enif_make_string(task->env, "Failed to decompress",
                ERL_NIF_LATIN1));
        goto done;
    }

    result = enif_make_tuple3(task->env, atom_ok, task->ref,
        enif_make_binary(task->env, &bin));

done:
    return result;
}

void*
worker(void *arg)
{
    ctx_t  *ctx;
    task_t *task;

    ERL_NIF_TERM result;

    ctx = static_cast<ctx_t*>(arg);

    while (true) {
        task = static_cast<task_t*>(async_queue_pop(ctx->queue));

        if (task->type == COMPRESS) {
            result = compress(task);
        } else if (task->type == DECOMPRESS) {
            result = decompress(task);
        } else if (task->type == SHUTDOWN) {
            break;
        } else {
            errx(1, "Unexpected task type: %i", task->type);
        }

        enif_send(NULL, &task->pid, task->env, result);
        cleanup_task(&task);
    }

    cleanup_task(&task);
    return NULL;
}

ctx_t*
init_ctx()
{
    int status;

    ctx_t *ctx = static_cast<ctx_t*>(enif_alloc_resource(res_type, sizeof(*ctx)));

    if (ctx == NULL) {
        goto done;
    }

    ctx->queue = async_queue_create();
    ctx->topts = enif_thread_opts_create(const_cast<char*>("snappy_thread_opts"));

    status = enif_thread_create(const_cast<char*>("worker"), 
        &ctx->tid, worker, ctx, ctx->topts);

    if (status != 0) {
        enif_release_resource(ctx);
        ctx = NULL;
        goto done;
    }

done:
    return ctx;
}

void
resource_dtor(ErlNifEnv *env, void *obj)
{
    ctx_t  *ctx = static_cast<ctx_t*>(obj);
    task_t *task = init_empty_task(SHUTDOWN);
    void   *result = NULL;

    async_queue_push(ctx->queue, static_cast<void*>(task));
    enif_thread_join(ctx->tid, &result);
    async_queue_destroy(ctx->queue);
    enif_thread_opts_destroy(ctx->topts);
}

int
load(ErlNifEnv *env, void **priv, ERL_NIF_TERM load_info)
{
    const char *mod = "erlang-snappy";
    const char *name = "nif_resource";

    ErlNifResourceFlags flags = static_cast<ErlNifResourceFlags>(ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER);

    res_type = enif_open_resource_type(env, mod, name, resource_dtor, flags, NULL);

    if (res_type == NULL) {
        return -1;
    }

    atom_ok = enif_make_atom(env, "ok");
    atom_error = enif_make_atom(env, "error");

    return 0;
}

ERL_NIF_TERM
snappy_create_ctx(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    ERL_NIF_TERM  rv;
    ctx_t        *ctx;

    ctx = init_ctx();

    if (ctx == NULL) {
        return enif_make_tuple2(env, atom_error,
            enif_make_string(env, "Failed to create context",
                ERL_NIF_LATIN1));
    }

    rv = enif_make_resource(env, ctx);
    enif_release_resource(ctx);

    return enif_make_tuple2(env, atom_ok, rv);
}

ERL_NIF_TERM
snappy_compress_impl(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    ctx_t  *ctx;
    task_t *task;

    ErlNifPid pid;

    if (argc != 4) {
        return enif_make_badarg(env);
    }

    if (!enif_get_resource(env, argv[0], res_type,
            reinterpret_cast<void**>(&ctx))) {
        return enif_make_badarg(env);
    }

    if (!enif_is_ref(env, argv[1])) {
        return enif_make_tuple2(env, atom_error,
            enif_make_string(env, "Second arg. is not a reference",
                ERL_NIF_LATIN1));
    }

    if (!enif_get_local_pid(env, argv[2], &pid)) {
        return enif_make_tuple2(env, atom_error,
            enif_make_string(env, "Third arg. is not a pid of local process",
                ERL_NIF_LATIN1));
    }

    if (!enif_is_binary(env, argv[3])) {
        return enif_make_tuple2(env, atom_error,
            enif_make_string(env, "Forth arg. is not a binary",
                ERL_NIF_LATIN1));
    }

    task = init_task(COMPRESS, argv[1], pid, argv[3]);

    if (!task) {
        return enif_make_tuple2(env, atom_error,
            enif_make_string(env, "Failed to create a task",
                ERL_NIF_LATIN1));
    }

    async_queue_push(ctx->queue, static_cast<void*>(task));

    return atom_ok;
}

ERL_NIF_TERM
snappy_decompress_impl(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    ctx_t  *ctx;
    task_t *task;

    ErlNifPid pid;

    if (argc != 4) {
        return enif_make_badarg(env);
    }

    if (!enif_get_resource(env, argv[0], res_type,
            reinterpret_cast<void**>(&ctx))) {
        return enif_make_badarg(env);
    }

    if (!enif_is_ref(env, argv[1])) {
        return enif_make_tuple2(env, atom_error,
            enif_make_string(env, "Second arg. is not a reference",
                ERL_NIF_LATIN1));
    }

    if (!enif_get_local_pid(env, argv[2], &pid)) {
        return enif_make_tuple2(env, atom_error,
            enif_make_string(env, "Third arg. is not a pid of local process",
                ERL_NIF_LATIN1));
    }

    if (!enif_is_binary(env, argv[3])) {
        return enif_make_tuple2(env, atom_error,
            enif_make_string(env, "Forth arg. is not a binary",
                ERL_NIF_LATIN1));
    }

    task = init_task(DECOMPRESS, argv[1], pid, argv[3]);

    if (!task) {
        return enif_make_tuple2(env, atom_error,
            enif_make_string(env, "Failed to create a task",
                ERL_NIF_LATIN1));
    }

    async_queue_push(ctx->queue, static_cast<void*>(task));

    return atom_ok;
}

static ErlNifFunc nif_funcs[] = {
    {"create_ctx", 0, snappy_create_ctx},
    {"compress_impl", 4, snappy_compress_impl},
    {"decompress_impl", 4, snappy_decompress_impl}
};

ERL_NIF_INIT(esnappy, nif_funcs, &load, NULL, NULL, NULL);

} // extern "C"
