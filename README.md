## Description

The **esnappy** library provides Erlang bindings to Google's
[Snappy compression library](http://code.google.com/p/snappy/).
It uses separate OS thread for compression/decompression so it won't
screw up Erlang's VM scheduler while processing large data chunks.

## Erlang Version

The **esnappy** library requires Erlang R14B or later.

## Building

You have to have Snappy library installed on your system so that
compiler can link against it. You can also specify **ESNAPPY_INCLUDE_DIR**
and **ESNAPPY_LIB_DIR** enviroment variables for better control of
paths used to compile and link **esnappy** library.

<pre>
$ ESNAPPY_INCLUDE_DIR=/usr/local/include ESNAPPY_LIB_DIR=/usr/local/lib ./rebar compile
</pre>

## Perfomance

<pre>
Erlang R14B02 (erts-5.8.3) [source] [64-bit] [smp:4:4] [rq:4] [async-threads:0] [hipe] [kernel-poll:false]

Eshell V5.8.3  (abort with ^G)
1> code:add_path("ebin").
true
2> {ok, Data} = file:read_file("test/text.txt").
{ok,&lt;&lt;32,32,32,208,155,208,181,208,178,32,208,157,208,
      184,208,186,208,190,208,187,208,176,208,181,208,
      178,208,...&gt;&gt;}
3> {ok, Ctx} = esnappy:create_ctx().
{ok,&lt;&lt;&gt;&gt;}
4> {ST, {ok, SCompressed}} = timer:tc(esnappy, compress, [Ctx, Data]).
{46692,
 {ok,&lt;&lt;217,192,187,1,84,32,32,32,208,155,208,181,208,178,
       32,208,157,208,184,208,186,208,190,208,187,...&gt;&gt;}}
5> {ZT, ZCompressed} = timer:tc(zlib, zip, [Data]).
{493585,
 &lt;&lt;172,189,203,110,37,219,145,37,56,207,175,56,17,147,146,
   10,78,126,64,196,128,227,4,106,212,104,52,...&gt;&gt;}
6> size(Data).
3072089
7> size(SCompressed).
1548017
8> size(ZCompressed).
832898
</pre>

Note the difference in execution time **46692** (Snappy) vs. **493585** (zlib).
