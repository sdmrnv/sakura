local sakura = require "resty.sakura"
local c_time = require "c_time"

local START = c_time.c_gettimeofday()

local oracle, err = sakura:new()
if not oracle then 
  ngx.say("failed to instantiate oracle=>", (err or "NULL"))
  return
end

----ora:set_timeout(1000)

--local ok, err = ora:connect{user="pax", password="pax", pool_size = 1000, backlog = 1000}
local ok, err = oracle:connect{user="pax", password="pax"}
if not ok then 
  ngx.say("CONNECT ERROR=>"..(err or "NULL")) 
  return
end


local data, err = oracle:query("select n, praenomen, brevis, dsc, to_char(fd,'dd.mm.yyyy hh24:mi:ss') from praenomen_masc where sysdate between fd and td order by n")
--local data, err = ora:query("select n, praenomen, brevis, dsc, to_char(fd,'dd.mm.yyyy hh24:mi:ss') from praenomen_masc where sysdate between fd and td and rownum<2")
--local data, err = ora:query("select f2 from t1 where f1=7")
if not data then 
  ngx.say("SELECT ERROR=>"..(err or "NULL")) 
  return
end

-- only about several updates occured (?due locking record???)  
--[[
local data, err = oracle:query("insert into sakura(n) values (1)")
if not data then 
  ngx.say("DML ERROR=>"..(err or "NULL")) 
  return
end
--]]

--[[
local ok, err = oracle:commit()
if not ok then 
  ngx.say("COMMIT ERROR=>"..(err or "NULL")) 
  return
end
--]]

--[[
local ok, err = oracle:rollback()
if not ok then 
  ngx.say("ROLLBACK ERROR=>"..(err or "NULL")) 
  return
end
--]]

--[[
local data, err = oracle:query("drop table t8")
if not data then 
  ngx.say("DDL ERROR=>"..(err or "NULL")) 
  return
end
--]]

local ok, err = oracle:disconnect()
if not ok then 
  ngx.say("DISCONNECT ERROR=>"..(err or "NULL")) 
  return
end


--[[
local ok, err = oracle:set_keepalive(10000, 1000)
if not ok then
  ngx.say("SET_KEEPALIVE ERROR=>"..(err or "NULL"))
  return
end
--]]


local HTML=""
for i=1,#data do
  HTML = HTML..data[i][1]
  HTML = HTML..data[i][2]
  HTML = HTML..data[i][3]
  HTML = HTML..data[i][4]
  HTML = HTML..data[i][5]
end


local FINISH = c_time.c_gettimeofday()
local DELTA_MU = FINISH - START
local DELTA_MS = DELTA_MU/1000


ngx.say(HTML)