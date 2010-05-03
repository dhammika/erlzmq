%%%-------------------------------------------------------------------
%%% @doc
%%% Erlang bindings for ZeroMQ.
%%% ------------------------------------------------------------------
%%% "THE BEER-WARE LICENSE"
%%%  <dhammika@gmail.com> wrote this file. As long as you retain this 
%%%  notice you can do whatever you want with this stuff. If we meet 
%%%  some day, and you think this stuff is worth it, you can buy me a 
%%%  beer in return.
%%% ------------------------------------------------------------------
%%% @end
%%%-------------------------------------------------------------------
-module(zmq).
-author("dhammika@gmail.com").

-behaviour(gen_server).

%% ZMQ API
-export([start_link/0, init/3, term/0,
         socket/1, close/1, sockopt/2, bind/2, connect/2,
	     send/2, recv/1]).

%% gen_server callbacks.
-export([init/1,
         handle_call/3, handle_cast/2, handle_info/2,
	     terminate/2, code_change/3]).

-define('DRIVER_NAME', 'zmq_drv').
-record(state, {port}).

%% ZMQ socket types.
-define('ZMQ_P2P', 0).
-define('ZMQ_PUB', 1).
-define('ZMQ_SUB', 2).
-define('ZMQ_REQ', 3).
-define('ZMQ_REP', 4).
-define('ZMQ_XREQ', 5).
-define('ZMQ_XREP', 6).
-define('ZMQ_UPSTREAM', 7).
-define('ZMQ_DOWNSTREAM', 8).

%% ZMQ socket options.
-define('ZMQ_HWM', 1).
-define('ZMQ_LWM', 2).
-define('ZMQ_SWAP', 3).
-define('ZMQ_AFFINITY', 4).
-define('ZMQ_IDENTITY', 5).
-define('ZMQ_SUBSCRIBE', 6).
-define('ZMQ_UNSUBSCRIBE', 7).
-define('ZMQ_RATE', 8).
-define('ZMQ_RECOVERY_IVL', 9).
-define('ZMQ_MCAST_LOOP', 10).
-define('ZMQ_SNDBUF', 11).
-define('ZMQ_RCVBUF', 12).
-define('ZMQ_RCVMORE', 13).

%% ZMQ send/recv options.
-define('ZMQ_NOBLOCK', 1).
-define('ZMQ_SNDMORE', 2).

%% ZMQ port options.
-define('ZMQ_INIT', 1).
-define('ZMQ_TERM', 2).
-define('ZMQ_SOCKET', 3).
-define('ZMQ_CLOSE', 4).
-define('ZMQ_SETSOCKOPT', 5).
-define('ZMQ_GETSOCKOPT', 6).
-define('ZMQ_BIND', 7).
-define('ZMQ_CONNECT', 8).
-define('ZMQ_SEND', 9).
-define('ZMQ_RECV', 10).

%% Debug log.
log(Msg, MsgArgs) ->
    io:format(string:concat(string:concat("[~p:~p] ", Msg), "~n"),
    [?MODULE, ?LINE | MsgArgs]).

%%%===================================================================
%%% ZMQ API
%%%===================================================================

%%--------------------------------------------------------------------
%% @doc
%% Start the server.
%%
%% @spec start_link() ->
%%          {ok, Pid} |
%%          {error, Error} |
%%          ignore
%% @end
%%--------------------------------------------------------------------
start_link() ->
    gen_server:start_link({local, ?MODULE}, ?MODULE, [], []).
init(AppThreads, IoThreads, Flags) ->
    gen_server:call(?MODULE, {init, AppThreads, IoThreads, Flags}).
term() ->
    gen_server:call(?MODULE, {term}).
socket(Type) ->
    gen_server:call(?MODULE, {socket, Type}).
close(Socket) ->
    gen_server:call(?MODULE, {close, Socket}).
sockopt(Command, {Socket, Option, Value}) ->
    gen_server:call(?MODULE, {sockopt, Command, {Socket, Option, Value}}).
%sockopt(Command, {Socket, Option}) ->
%    gen_server:call(?MODULE, {sockopt, Command, {Socket, Option}}).
bind(Socket, Address) ->
    gen_server:call(?MODULE, {bind, Socket, Address}).
connect(Socket, Address) ->
    gen_server:call(?MODULE, {connect, Socket, Address}).
send(Socket, Data) ->
    gen_server:call(?MODULE, {send, Socket, Data}).
recv(Socket) ->
    gen_server:call(?MODULE, {recv, Socket}).

%%%===================================================================
%%% gen_server callbacks
%%%===================================================================

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handle start.
%%
%% @spec init(Args) ->
%%          {ok, State} |
%%          {ok, State, Timeout} |
%%          {ok, State, hibernate} |
%%          {stop, Reason} |
%%          ignore
%% @end
%%--------------------------------------------------------------------
init([]) ->
    process_flag(trap_exit, true),
    SearchDir = filename:join(
            [filename:dirname(code:which(?MODULE)), "..", "priv"]),
    log("init, lib path:~p", [SearchDir]),
    case erl_ddll:load(SearchDir, atom_to_list(?DRIVER_NAME)) of
	    ok ->
	        {ok, #state{port=open_port({spawn, ?DRIVER_NAME}, [binary])}};
        {error, Reason} ->
            {stop, {error, Reason}}
    end.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handle synchronous call.
%%
%% @spec handle_call(Request, From, State) ->
%%          {reply, Reply, NewState} |
%%          {reply, Reply, NewState, Timeout} |
%%          {reply, Reply, NewState, hibernate} |
%%          {noreply, NewState} |
%%          {noreply, NewState, Timeout} |
%%          {noreply, NewState, hibernate} |
%%          {stop, Reason, Reply, NewState} |
%%          {stop, Reason, NewState}
%% @end
%%-------------------------------------------------------------------
handle_call({init, AppThreads, IoThreads, Flags}, _From, State) ->
    log("~p, app threads:~B io threads:~B",
        [init, AppThreads, IoThreads]),
    Message = <<(?ZMQ_INIT):32, AppThreads:32, IoThreads:32, Flags:32>>,
    Reply = driver(State#state.port, Message),
    {reply, Reply, State};

handle_call({term}, _From, State) ->
    log("~p", [term]),
    Message = <<(?ZMQ_TERM):32>>,
    Reply = driver(State#state.port, Message),
    {reply, Reply, State};

handle_call({socket, Type}, _From, State)
    when is_atom(Type) ->
        log("~p, type:~s", [socket, Type]),
        Message =
        case Type of
            zmq_p2p -> <<(?ZMQ_SOCKET):32, (?ZMQ_P2P):32>>;
            zmq_pub -> <<(?ZMQ_SOCKET):32, (?ZMQ_PUB):32>>;
            zmq_sub -> <<(?ZMQ_SOCKET):32, (?ZMQ_SUB):32>>;
            zmq_req -> <<(?ZMQ_SOCKET):32, (?ZMQ_REQ):32>>;
            zmq_rep -> <<(?ZMQ_SOCKET):32, (?ZMQ_REP):32>>;
            zmq_xreq -> <<(?ZMQ_SOCKET):32, (?ZMQ_XREQ):32>>;
            zmq_xrep -> <<(?ZMQ_SOCKET):32, (?ZMQ_XREP):32>>;
            zmq_upstream -> <<(?ZMQ_SOCKET):32, (?ZMQ_UPSTREAM):32>>;
            zmq_downstream -> <<(?ZMQ_SOCKET):32, (?ZMQ_DOWNSTREAM):32>>;
            other -> {error, "Unknown socket type"}
        end,
        Reply = driver(State#state.port, Message),
        {reply, Reply, State};

handle_call({close, Socket}, _From, State)
    when is_binary(Socket) ->
        log("~p", [close]),
        Message = <<(?ZMQ_CLOSE):32, Socket/binary>>,
        Reply = driver(State#state.port, Message),
        {reply, Reply, State};

% FIXME Doesn't support getsockopt yet.
handle_call({sockopt, _Command, {Socket, Option, Value}}, _From, State)
    when is_binary(Socket) ->
        log("~p", [socketopt]),
        Message =
        case Option of 
            zmq_hwm -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_HWM):32, Value/binary>>;
            zmq_lwn -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_LWM):32, Value/binary>>;
            zmq_swap -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_SWAP):32, Value/binary>>;
            zmq_affinity -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_AFFINITY):32, Value/binary>>;
            zmq_identity -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_IDENTITY):32, Value/binary>>;
            zmq_subscribe -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_SUBSCRIBE):32, Value/binary>>;
            zmq_unsubscibe -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_UNSUBSCRIBE):32, Value/binary>>;
            zmq_rate -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_RATE):32, Value/binary>>;
            zmq_racovery_ivl -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_RECOVERY_IVL):32, Value/binary>>;
            zmq_mcast_loop -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_MCAST_LOOP):32, Value/binary>>;
            zmq_rcvbuf -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_RCVBUF):32, Value/binary>>;
            zmq_rcvmore -> <<(?ZMQ_SETSOCKOPT):32, Socket/binary, (?ZMQ_RCVMORE):32, Value/binary>>;
            other -> {error, "Unknown socket type"}
        end,
        Reply = driver(State#state.port, Message),
        {reply, Reply, State};

handle_call({bind, Socket, Address}, _From, State)
    when is_binary(Socket) ->
        log("~p addr:~s", [bind, binary_to_term(Address)]),
        Message = <<(?ZMQ_BIND):32, Socket/binary, Address/binary>>,
        Reply = driver(State#state.port, Message),
        {reply, Reply, State};

handle_call({connect, Socket, Address}, _From, State)
    when is_binary(Socket) and is_binary(Address) ->
        log("~p addr:~s", [connect, binary_to_term(Address)]),
        Message = <<(?ZMQ_CONNECT):32, Socket/binary, Address/binary>>,
        Reply = driver(State#state.port, Message),
        {reply, Reply, State};

handle_call({send, Socket, Data}, _From, State)
    when is_binary(Socket) and is_binary(Data) ->
        log("~p", [send]),
        Message = <<(?ZMQ_SEND):32, Socket/binary, Data/binary>>,
        Reply = driver(State#state.port, Message),
        {reply, Reply, State};

handle_call({recv, Socket}, _From, State)
    when is_binary(Socket) ->
        log("~p", [recv]),
        Message = <<(?ZMQ_RECV):32, Socket/binary>>,
        Reply = driver(State#state.port, Message),
        {reply, Reply, State};

handle_call(_Request, _From, State) ->
    log("~p", ['unknown request']),
    Reply = {error, unkown_call},
    {reply, Reply, State}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handle asynchronous call.
%%
%% @spec handle_cast(Msg, State) ->
%%          {noreply, NewState} |
%%          {noreply, NewState, Timeout} |
%%          {noreply, NewState, hibernate} |
%%          {stop, Reason, NewState}
%% @end
%%--------------------------------------------------------------------
handle_cast(_Msg, State) ->
    {noreply, State}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handle timeout.
%%
%% @spec handle_info(Info, State) ->
%%          {noreply, NewState} |
%%          {noreply, NewState, Timeout} |
%%          {noreply, NewState, hibernate} |
%%          {stop, Reason, NewState}
%% @end
%%--------------------------------------------------------------------
handle_info(_Info, State) ->
    {noreply, State}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handle termination/shutdown.
%%
%% @spec terminate(Reason, State) -> void()
%% @end
%%--------------------------------------------------------------------
terminate(_Reason, State) ->
    port_close(State#state.port),
    ok.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handle code change.
%%
%% @spec code_change(OldVsn, State, Extra) -> 
%%          {ok, NewState}
%% @end
%%--------------------------------------------------------------------
code_change(_OldVsn, State, _Extra) ->
    {ok, State}.

%%%===================================================================
%%% zmq_drv port wrapper.
%%%===================================================================
driver(Port, Message) ->
    log("port command ~p", [Message]),
    port_command(Port, Message),
    receive
	    Data ->
	        Data
    end.
