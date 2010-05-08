% Basic REQ/REP client for testing.

-module(zmq_reqclient).
-export([run/0]).

run() ->
    zmq:start_link(),
    zmq:init(1,1,0),
    case zmq:socket(zmq_req) of
        {ok, Socket} ->
            zmq:connect(Socket, term_to_binary("tcp://127.0.0.1:5550")),
            reqrep(Socket, 1);
        other -> other
    end.

reqrep(Socket, MsgIndex) ->
    send(Socket, MsgIndex),
    recv(Socket),
    timer:sleep(1000),
    reqrep(Socket, MsgIndex + 1).

send(Socket, MsgIndex) ->
    Data = lists:flatten(io_lib:format("Msg ~B", [MsgIndex])),
    case zmq:send(Socket, list_to_binary(Data)) of
        ok ->
            io:format("Snd ~p ~n", [Data]);
        other -> other
    end.

recv(Socket) ->
    case zmq:recv(Socket) of
        {ok, Data} ->
            io:format("Rcv ~p ~n", [binary_to_list(Data)]);
        other -> other
    end.
