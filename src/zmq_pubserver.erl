% Basic PUB/SUB server for testing.

-module(zmq_pubserver).
-export([run/0]).

run() ->
    zmq:start_link(),
    zmq:init(1,1,0),
    case zmq:socket(zmq_pub) of
        {ok, Socket} -> 
            zmq:bind(Socket, term_to_binary("tcp://127.0.0.1:5550")),
            send(Socket);
        other -> other
    end.

send(Socket) ->
    send(Socket, 1).

send(Socket, MsgIndex) ->
    Data = lists:flatten(io_lib:format("Msg ~B", [MsgIndex])),
    io:format("Snd ~p ~n", [Data]),
    case zmq:send(Socket, list_to_binary(Data)) of 
        ok -> 
            sleep(1000),
            send(Socket, MsgIndex + 1);
        other -> other
    end.

sleep(MilliSecs) ->
    receive 
    after MilliSecs -> true
    end.
