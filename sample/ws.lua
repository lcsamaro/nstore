local tcp = ngx.socket.tcp

local server = require "resty.websocket.server"

local wb, err = server:new {
  timeout = 5000,
  max_payload_len = 512 * 1024,
}

if not wb then
  ngx.log(ngx.ERR, "failed to new websocket: ", err)
  return ngx.exit(444)
end

-- store setup
local sock, err = tcp()
if not sock then
  return nil, err
end

if not sock:connect("127.0.0.1", 4444) then
  return ngx.exit(444)
end

local sent = sock:send("namespace 0\r\n")
if not sent then return ngx.exit(444) end

local line, err = sock:receive()
if not line then ngx.exit(444) end

-- sub
local sent = sock:send("subscribe 0\r\n")
if not sent then return ngx.exit(444) end

local line, err = sock:receive()
if not line then ngx.exit(444) end

sock:settimeout(60000*10)  -- change the network timeout to 10 minutes

function trd_ws_read()
	wb:set_timeout(60000*10)  -- change the network timeout to 10 minutes

	while true do
		local data, typ, err = wb:recv_frame()

		if not data then
		  ngx.log(ngx.ERR, "failed to receive a frame: ", err)
		  return ngx.exit(444)
		end

		if typ == "close" then
		  -- send a close frame back:

		  local bytes, err = wb:send_close(1000, "enough, enough!")
		  if not bytes then
		    ngx.log(ngx.ERR, "failed to send the close frame: ", err)
		    return
		  end
		  local code = err
		  ngx.log(ngx.INFO, "closing with status code ", code, " and message ", data)
		  return
		end

		if typ == "ping" then
		  local bytes, err = wb:send_pong(data)
		  if not bytes then
		    ngx.log(ngx.ERR, "failed to send frame: ", err)
		    return
		  end
		elseif typ == "pong" then
		elseif typ == "text" then
		  --local bytes, err = wb:send_text(data)
		  --if not bytes then
		  --  ngx.log(ngx.ERR, "failed to send text: ", err)
		  --  return ngx.exit(444)
		  --end


	  	  -- make request to nstore
		  local sent = sock:send(data)
		  if not sent then return ngx.exit(444) end


		else
		  ngx.log(ngx.INFO, "received a frame of type ", typ, " and payload ", data)
		end
	end
end
ngx.thread.spawn(trd_ws_read)


while true do
  local line, err = sock:receive()
  if not line then
    if err == "timeout" then
      sock:close()

      local bytes, err = wb:send_close(1000, "enough, enough!")
      if not bytes then
        ngx.log(ngx.ERR, "failed to send the close frame: ", err)
        return
      end
    end
    ngx.log(ngx.ERR, "nstore conn closed", err)
    return ngx.exit(444)
  else
    local bytes, err = wb:send_text(line)
    if not bytes then
      ngx.log(ngx.ERR, "failed to send text: ", err)
      return ngx.exit(444)
    end
  end
end

