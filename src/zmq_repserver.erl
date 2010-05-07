% Basic REQ/REP server for testing.

-module(zmq_repserver).
-export([run/0]).

run() ->
    zmq:start_link(),
    zmq:init(1,1,0),
    case zmq:socket(zmq_rep) of
        {ok, Socket} ->
            zmq:bind(Socket, term_to_binary("tcp://127.0.0.1:5550")),
            reqrep(Socket);
        other -> other
    end.

reqrep(Socket) ->
    case zmq:recv(Socket) of
        {ok, Data} ->
            io:format("Rcv ~p ~n", [binary_to_list(Data)]),
            zmq:send(Socket, Data),
            io:format("Snd ~p ~n", [binary_to_list(Data)]);
        other -> other
    end,
    sleep(1000),
    reqrep(Socket).

sleep(MilliSecs) ->
    receive 
    after MilliSecs -> true
    end.
