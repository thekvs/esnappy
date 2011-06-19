%%% @author Konstantin Sorokin <kvs@sigterm.ru>
%%%
%%% @copyright 2011 Konstantin V. Sorokin, All rights reserved. Open source, BSD License
%%% @version 1.0
%%%
-module(esnappy).
-version(1.0).
-on_load(init/0).
-export([create_ctx/0, compress/2, decompress/2]).

-ifdef(TEST).
-include_lib("eunit/include/eunit.hrl").
-endif.

%% @doc Initialize NIF.
init() ->
    SoName = filename:join(case code:priv_dir(?MODULE) of
                               {error, bad_name} ->
                                   %% this is here for testing purposes
                                   filename:join(
                                     [filename:dirname(
                                        code:which(?MODULE)),"..","priv"]);
                               Dir ->
                                   Dir
                           end, atom_to_list(?MODULE) ++ "_nif"),
    erlang:load_nif(SoName, 0).

compress_impl(_Ctx, _Ref, _Self, _IoList) ->
    erlang:nif_error(not_loaded).

decompress_impl(_Ctx, _Ref, _Self, _IoList) ->
    erlang:nif_error(not_loaded).

create_ctx() ->
    erlang:nif_error(not_loaded).

compress(Ctx, RawData) ->
    Ref = make_ref(),
    ok = compress_impl(Ctx, Ref, self(), RawData),
    receive
        {ok, Ref, CompressedData} ->
            {ok, CompressedData};
        {error, Reason} ->
            {error, Reason};
        Other ->
            throw(Other)
    end.

decompress(Ctx, CompressedData) ->
    Ref = make_ref(),
    ok = decompress_impl(Ctx, Ref, self(), CompressedData),
    receive
        {ok, Ref, UncompressedData} ->
            {ok, UncompressedData};
        {error, Reason} ->
            {error, Reason};
        Other ->
            throw(Other)
    end.

%% ===================================================================
%% EUnit tests
%% ===================================================================
-ifdef(TEST).

esnappy_test() ->
    {ok, Ctx} = create_ctx(),
    {ok, Data} = file:read_file("../test/text.txt"),
    {ok, CompressedData} = compress(Ctx, Data),
    {ok, UncompressedData} = decompress(Ctx, CompressedData),
    true = (Data =:= UncompressedData andalso
        size(CompressedData) < size(Data)).

esnappy_iolist_test() ->
    {ok, Ctx} = create_ctx(),
    RawData = ["fshgggggggggggggggggg", <<"weqeqweqw">>],
    {ok, CompressedData} = compress(Ctx, RawData),
    {ok, UncompressedData} = decompress(Ctx, CompressedData),
    true = (list_to_binary(RawData) =:= UncompressedData).

-endif.
