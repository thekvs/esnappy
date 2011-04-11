## Description

The **esnappy** library provides Erlang bindings to Google's
[Snappy compression library](http://code.google.com/p/snappy/).
It uses separate thread for compression/decompression so it won't
screw up Erlang's VM scheduler while processing large binaries.
Code of the Snappy library included in this distribution.

## Erlang Version

The **esnappy** library requires Erlang R14B or later.

## Perfomance

<pre>
Erlang R14B02 (erts-5.8.3) [source] [64-bit] [smp:4:4] [rq:4] [async-threads:0] [hipe] [kernel-poll:false]

Eshell V5.8.3  (abort with ^G)
1> code:add_path("ebin").
true
2> {ok, Data} = file:read_file("test/text.txt").
{ok,<<32,32,32,208,155,208,181,208,178,32,208,157,208,
      184,208,186,208,190,208,187,208,176,208,181,208,
      178,208,...>>}
3> {ok, Ctx} = esnappy:create_ctx().
{ok,<<>>}
4> {ST, {ok, SCompressed}} = timer:tc(esnappy, compress, [Ctx, Data]).
{46692,
 {ok,<<217,192,187,1,84,32,32,32,208,155,208,181,208,178,
       32,208,157,208,184,208,186,208,190,208,187,...>>}}
5> {ZT, ZCompressed} = timer:tc(zlib, zip, [Data]).
{493585,
 <<172,189,203,110,37,219,145,37,56,207,175,56,17,147,146,
   10,78,126,64,196,128,227,4,106,212,104,52,...>>}
6> size(Data).
3072089
7> size(SCompressed).
1548017
8> size(ZCompressed).
832898
</pre>
