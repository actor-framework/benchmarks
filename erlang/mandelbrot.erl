-module(mandelbrot).
-export([start/1]).
-define(LIM_SQR, 4.0).
-define(ITER, 250).
-define(SR, -1.5).
-define(SI, -1).

start(X) ->
    [H0|_] = X,
    N = list_to_integer(atom_to_list(H0)),
    %% Spawn one process per row
    Row = fun(Y)-> spawn(fun()-> row(0, ?SI+Y*2/N, N, 0, [], 7) end) end,
    Pids = lists:map(Row, lists:seq(0,N-1)),
    %Pass token around to make sure printouts are in the right order
    hd(Pids) ! tl(Pids) ++ [ self() ],
    receive _Token -> halt(0) end.

%Iterate over a row, collect bits, bytes and finally print the row
row(X, _, N, Bits, Bytes, BitC) when X =:= N-1 ->
    receive Pids ->
        hd(Pids) ! tl(Pids)
    end;

row(X, Y2, N, Bits, Bytes, 0) ->
    row(X+1, Y2, N, 0, [Bits bsl 1 + m(?ITER, ?SR+X*2/N, Y2) | Bytes], 7);

row(X, Y2, N, Bits, Bytes, BitC) ->
    row(X+1, Y2, N, Bits bsl 1 + m(?ITER, ?SR+X*2/N, Y2), Bytes, BitC-1).

%Mandelbrot algorithm
m(Iter, CR,CI) -> m(Iter - 1, CR, CI, CR, CI).

m(Iter, R, I, CR, CI) ->
    case R*R+I*I > ?LIM_SQR of
    false when Iter > 0 -> m(Iter-1, R*R-I*I+CR, 2*R*I+CI, CR, CI);
    false -> 1;
    true -> 0
    end.
