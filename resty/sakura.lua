local tcp = ngx.socket.tcp
local strbyte = string.byte
local bit = require "bit"
local bor = bit.bor
local lshift = bit.lshift
local band = bit.band
local rshift = bit.rshift
local strchar = string.char
local setmetatable = setmetatable
local cjson = require "cjson.safe"

local _M = {}

local SOC_FILENAME  =  "/var/run/soc"
local DB_NAME       =  "localhost:1521/orclcdb"

local STATE_CONNECTED    = 1
local STATE_COMMAND_SENT = 2

local mt = { __index = _M }

local function _set_byte4(n)
    return strchar(band(n, 0xff),
                   band(rshift(n, 8), 0xff),
                   band(rshift(n, 16), 0xff),
                   band(rshift(n, 24), 0xff))
end

local function _get_byte4(data)
    local a, b, c, d = strbyte(data, 1, 4)
    return bor(a, lshift(b, 8), lshift(c, 16), lshift(d, 24))
end


local function  _trim(s)
   return s:match'^%s*(.*%S)' or ''
end

function _M.new(self)
  local soc, err = tcp()
  if not soc then
    return nil, "Error when open soc=>"..(err or "NULL")
  end
  return setmetatable({ soc = soc }, mt)
end


function _M.set_timeout(self, timeout)
    local soc = self.soc
    if not soc then
      return false, "soc not initialized"
    end

    return soc:settimeout(timeout)
end


function _M.connect(self, opts)
  local soc = self.soc
  if not soc then
    return false, "soc not initialized"
  end


  self.database = opts.database or DB_NAME
  self.user = opts.user or ""
  self.password = opts.password or ""

  local pool = opts.pool
  if not pool then
    pool = self.user .. ":" .. self.database .. ":" .. SOC_FILENAME
  end

  local ok; local err

  ok, err = soc:connect("unix:"..SOC_FILENAME, {pool = pool,
                               pool_size = opts.pool_size,
                               backlog = opts.backlog })
  if not ok then
    return false, "Error when connect to soc=>"..(err or "null")
  end

  local reused = soc:getreusedtimes()
  if reused and reused > 0 then
    self.state = STATE_CONNECTED
    return true, nil
  end

  local con_string = self.user.."/"..self.password.."@"..self.database
  ok, err = soc:send("SOC".._set_byte4(#con_string)..con_string)
  if not ok then
    soc:close()
    return false, "Error when send into soc=>"..(err or "null")
  end

  --socket answer may be (or may be not) deleted due notable memory leak problem under highload by wrk (e.g. 3 cycles with -c500 -d30s) that generate soket errors 
  --for example - Socket errors: connect 0, read 0, write 0, timeout 165
  --without answer a litle bit memory leak also takes place -:((
  --this problem needs additional reserch effort
 
  local status; local len; local len_num; local data
  status, err = soc:receive(3); if not status then soc:close(); return false, "Error when recieve status from soc=>"..(err or "null") end
  len, err = soc:receive(4);  if not len then soc:close(); return false, "Error when recieve len from soc=>"..(err or "null") end
  len_num = _get_byte4(len)
  data, err = soc:receive(len_num); if not data then soc:close(); return false, "Error when recieve packet from soc=>"..(err or "null") end
  if (status ~= 'SOO') then soc:close(); return false, "FAILED=>"..(data or "null") end
  

  self.state = STATE_CONNECTED
  return true, nil

end




function _M.disconnect(self)
  local soc = self.soc
  if not soc then
    return false, "soc not initialized"
  end

  local ok; local err

  self.state = nil

  ok, err = soc:send("SOD")
  if not ok then
    soc:close()
    return false, "Error when send into soc=>"..(err or "null")
  end

  local status; local len; local len_num; local data
  status, err = soc:receive(3); if not status then soc:close(); return false, "Error when recieve status from soc=>"..(err or "null") end
  len, err = soc:receive(4);  if not len then soc:close(); return false, "Error when recieve len from soc=>"..(err or "null") end
  len_num = _get_byte4(len)
  data, err = soc:receive(len_num); if not data then soc:close(); return false, "Error when recieve packet from soc=>"..(err or "null") end
  if (status ~= 'SOO') then soc:close(); return false, "FAILED=>"..(data or "null") end

  ok, err = soc:close()
  if not ok then
    return false, "Error when close soc=>"..(err or "null")
  end

  return true, nil
end


function _M.set_keepalive(self, ...)
    local soc = self.soc
    if not soc then
      return false, "soc not initialized"
    end

    if self.state ~= STATE_CONNECTED then
      return false, "soc cannot be reused in the current connection state=>".. (self.state or "NULL")
    end  local ok; local err


    self.state = nil
    return soc:setkeepalive(...)
end


function _M.query(self, query_string)
  local soc = self.soc
  if not soc then
    return false, "soc not initialized"
  end

  if self.state ~= STATE_CONNECTED then
    return false, "soc cannot be reused in the current connection state=>".. (self.state or "NULL")
  end
  
  if not query_string then 
    return false, "NULL Query"
  end

  query_string = _trim(query_string)  
  local query_typ
  if (string.upper(string.sub(query_string,1,6)) == "SELECT") then query_typ = 'SOS' else query_typ = 'SOQ' end


  local ok; local err

  ok, err = soc:send(query_typ.._set_byte4(#query_string)..query_string)
  if not ok then
    soc:close()
    return false, "Error when send into soc=>"..(err or "null")
  end

  self.state = STATE_COMMAND_SENT

  local status; local len; local len_num; local data
  status, err = soc:receive(3); if not status then soc:close(); return false, "Error when recieve status from soc=>"..(err or "null") end
  len, err = soc:receive(4);  if not len then soc:close(); return false, "Error when recieve len from soc=>"..(err or "null") end
  len_num = _get_byte4(len)
  data, err = soc:receive(len_num); if not data then soc:close(); return false, "Error when recieve packet from soc=>"..(err or "null") end
  if (status ~= 'SOO') then soc:close(); return false, "FAILED=>"..(data or "null") end

  self.state = STATE_CONNECTED
  if query_typ == 'SOS' then 
    local data_t = cjson.decode(data)
    return data_t, nil
  else
    return true, nil
  end
    
end



function _M.commit(self)
  local soc = self.soc
  if not soc then
    return false, "soc not initialized"
  end

  local ok; local err

  ok, err = soc:send("SOT")
  if not ok then
    soc:close()
    return false, "Error when send into soc=>"..(err or "null")
  end

  local status; local len; local len_num; local data
  status, err = soc:receive(3); if not status then soc:close(); return false, "Error when recieve status from soc=>"..(err or "null") end
  len, err = soc:receive(4);  if not len then soc:close(); return false, "Error when recieve len from soc=>"..(err or "null") end
  len_num = _get_byte4(len)
  data, err = soc:receive(len_num); if not data then soc:close(); return false, "Error when recieve packet from soc=>"..(err or "null") end
  if (status ~= 'SOO') then soc:close(); return false, "FAILED=>"..(data or "null") end

  return true, nil
end


function _M.rollback(self)
  local soc = self.soc
  if not soc then
    return false, "soc not initialized"
  end

  local ok; local err

  ok, err = soc:send("SOR")
  if not ok then
    soc:close()
    return false, "Error when send into soc=>"..(err or "null")
  end

  local status; local len; local len_num; local data
  status, err = soc:receive(3); if not status then soc:close(); return false, "Error when recieve status from soc=>"..(err or "null") end
  len, err = soc:receive(4);  if not len then soc:close(); return false, "Error when recieve len from soc=>"..(err or "null") end
  len_num = _get_byte4(len)
  data, err = soc:receive(len_num); if not data then soc:close(); return false, "Error when recieve packet from soc=>"..(err or "null") end
  if (status ~= 'SOO') then soc:close(); return false, "FAILED=>"..(data or "null") end

  return true, nil
end


return _M