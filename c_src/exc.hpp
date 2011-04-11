#ifndef __EXC_HPP_INCLUDED__
#define __EXC_HPP_INCLUDED__

#include <exception>

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cerrno>

class exc: public std::exception {
public:

    exc(const char *file, int line, const char *fmt, ...): msg(NULL)
    {
        va_list ap;
        char    buff[1024];
        size_t  len;

        va_start(ap, fmt);
        vsnprintf(buff, sizeof(buff), fmt, ap);
        va_end(ap);

        len = 0;
        memset(line_str, 0, sizeof(line_str));
        snprintf(line_str, sizeof(line_str), "%i", line);
        len = strlen(buff) + 4 /* ' at ' */ + strlen(file) + 1 /* ':' */ +
            strlen(line_str) + 1 /* terminating zero */;
        msg = static_cast<char*>(calloc(1, len));
        snprintf(msg, len, "%s at %s:%s", buff, sanitize_file_name(file),
            line_str);
    }

    virtual ~exc() throw() {
        free(msg);
    };

    virtual const char *what() const throw() {
        return msg == NULL ? "" : msg;
    }

private:

    char *msg;
    char  line_str[sizeof("2147483647")];

    const char* sanitize_file_name(const char *file)
    {
        const char *s;
        char       *p;

        p = const_cast<char*>(strrchr(file, '/'));

        if (p == NULL) {
            s = file;
        } else {
            p++;
            s = p;
        }

        return s;
    }
};

#define THROW_EXC(args...)  (throw exc(__FILE__, __LINE__, args))

#define THROW_EXC_IF_FAILED(status, args...) do {       \
    if (!(status)) {                                    \
        throw exc(__FILE__, __LINE__, args);            \
    }                                                   \
} while(0)

#endif
