-- Utility functions for testing dblite.
-- Ben Poplawski


-- table.foreach
function E (t,f)
  f = f or print
  for k,v in pairs(t) do
    local x = f(k,v)
    if x ~= nil then return x end
  end
end


-- table.foreachi
function I (t,f)
  f = f or print
  for i=1,#t do
    local x = f(i,t[i])
    if x ~= nil then return x end
  end
end


function notnumber (k,v)
  if type(v) ~= "number" then
    print(k,v)
  end
end


function inserter (t,incl_names)
  local names_done = false
  return function (x,n,data,names)
    if incl_names and not names_done then
      names_done = true
      table.insert(t, names)
    end
    table.insert(t, data)
  end
end


function cattab (t)
  local res = {}
  for k,v in pairs(t) do
    table.insert(res, tostring(k).."="..tostring(v))
  end
  return "{"..table.concat(res,", ").."}"
end


function printv (...)
  local n, res = select("#", ...), {}
  for i=1,n do
    local v, r = select(i, ...)
    if type(v) == "table" then
      r = cattab(v)
    else
      r = tostring(v)
    end
    table.insert(res, r)
  end
  print(table.unpack(res))
end
