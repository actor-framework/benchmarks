-module(actor_creation).
-export([start/1, testee/1]).

testee(Pid) ->
    receive
        {spread, 1} ->
            Pid ! {result, 1};
        {spread, X} ->
            spawn(actor_creation, testee, [self()]) ! {spread, X-1},
            spawn(actor_creation, testee, [self()]) ! {spread, X-1},
            receive
                {result, R1} ->
                    receive
                        {result, R2} ->
                            Pid ! {result, (1+R1+R2)}
                    end
            end
    end.

start(X) ->
    [H|_] = X,
    N = list_to_integer(atom_to_list(H)),
    spawn(actor_creation, testee, [self()]) ! {spread, N},
    receive
        {result, R} ->
            if
                (1 + R) == (1 bsl N) ->
                    ok;
                true ->
                    error("unexpected result!")
            end
    end.
