% Basic client for testing.

-module(zmqclient).
-export([run/0]).

run() ->
    zmq:start_link(),
    zmq:init(1,1,0),
    case zmq:socket(zmq_sub) of
        {ok, Socket} -> 
            zmq:sockopt(set, {Socket, zmq_subscribe, <<>>}),
            zmq:connect(Socket, term_to_binary("tcp://127.0.0.1:5550")),
            read(Socket);
        other -> other
    end.

read(Socket) ->
    case zmq:recv(Socket) of 
        {ok, Data} -> 
            io:format("Got data ~p ~n", [binary_to_list(Data)]),
            read(Socket);
        other -> other
    end.
