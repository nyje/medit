-- slnunicode unit tests

require("munit")

--	there are four string-like ctype closures:
--	unicode.ascii, latin1, utf8 and grapheme
--
--	ascii and latin1 are single-byte like string,
--	but use the unicode table for upper/lower and character classes
--	ascii does not touch bytes > 127 on upper/lower
--
--	ascii or latin1 can be used as locale-independent string replacement.
--	(There is a compile switch to do this automatically for ascii).
--
--	UTF-8 operates on UTF-8 sequences as of RFC 3629:
--	1 byte 0-7F, 2 byte 80-7FF, 3 byte 800-FFFF, 4 byte 1000-10FFFF
--	(not exclusing UTF-16 surrogate characters)
--	Any byte not part of such a sequence is treated as it's (Latin-1) value.
--
--	Grapheme takes care of grapheme clusters, which are characters followed by
--	"grapheme extension" characters (Mn+Me) like combining diacritical marks.
--
--	calls are:
--	len(str)
--	sub(str, start [,end=-1])
--	byte(str, start [,end=-1])
--	lower(str)
--	upper(str)
--	char(i [,j...])
--	reverse(str)
--
--	same as in string: rep, format, dump
--	TODO: use char count with %s in format? (sub does the job)
--	TODO: grapheme.byte: only first code of any cluster?
--
--	find, gfind, gsub: done, but need thorough testing ...:
--	ascii does not match them on any %class (but on ., literals and ranges)
--	behaviour of %class with class not ASCII is undefined
--	frontier %f currently disabled -- should we?
--
--	character classes are:
--	%a L* (Lu+Ll+Lt+Lm+Lo)
--	%c Cc
--	%d 0-9
--	%l Ll
--	%n N* (Nd+Nl+No, new)
--	%p P* (Pc+Pd+Ps+Pe+Pi+Pf+Po)
--	%s Z* (Zs+Zl+Zp) plus the controls 9-13 (HT,LF,VT,FF,CR)
--	%u Lu (also Lt ?)
--	%w %a+%n+Pc (e.g. '_')
--	%x 0-9A-Za-z
--	%z the 0 byte
--	c.f. http://unicode.org/Public/UNIDATA/UCD.html#General_Category_Values
--	http://unicode.org/Public/UNIDATA/UnicodeData.txt
--
--	NOTE: find positions are in bytes for all ctypes!
--	use ascii.sub to cut found ranges!
--	this is a) faster b) more reliable
--
--	UTF-8 behaviour: match is by codes, code ranges are supported
--
--	grapheme behaviour: any %class, '.' and range match includes
--	any following grapheme extensions.
--	Ranges apply to single code points only.
--	If a [] enumeration contains a grapheme cluster,
--	this matches only the exact same cluster.
--	However, a literal single 'o' standalone or in an [] enumeration
--	will match just that 'o',	even if it has a extension in the string.
--	Consequently, grapheme match positions are not always cluster positions.
--

local unicode = {utf8=utf8, ascii=string, string=string}
local sprintf = string.format
local function printf (fmt, ...) return print(sprintf(fmt, ...)) end

local function check (test, ok, got)
  tassert(ok == got, "%s = %s GOT '%s'",test, ok, got or "<nil>")
end
local function checka (test, ok, ...)
	local arg = {...}
	arg[1] = arg[1] or ""
	return check(test, ok, table.concat(arg, ","))
end


local function testlen (str,bytes,codes)
	codes = codes or bytes
	return check(sprintf("len '%s'", str),
		sprintf("%d/%d", bytes, codes),
sprintf("%d/%d", string.len(str), utf8.len(str)))
end

-- 176 = 00B0;DEGREE SIGN -- UTF-8: C2,B0 = \194\176
-- 196 = 00C4;LATIN CAPITAL LETTER A WITH DIAERESIS
-- 214 = 00D6;LATIN CAPITAL LETTER O WITH DIAERESIS
-- 776 = 0308;COMBINING DIAERESIS -- UTF-8: CC,88 = \204\136
testlen("A\tB",3) -- plain Latin-1
testlen("°ÄÖ",6,3) -- simple Latin-1 chars in UTF-8
testlen("\204\136A\204\136O\204\136",8,5,3) -- decomposed (with broken lead)


local function testsub (ctype,ok,str,start,e)
	return check(sprintf("%s.sub('%s',%d,%d)", ctype, str, start, e), ok,
		unicode[ctype].sub(str,start,e))
end
testsub("ascii","BCD","ABCDE",2,4)
testsub("utf8","BCD","ABCDE",2,4)
testsub("utf8","Ä","°ÄÖ",2,2)
testsub("utf8","ÄÖ","°ÄÖ",2,-1)
testsub("utf8","\204\136","A\204\136O\204\136",2,2) -- decomposed


local function testbyte (ctype, ok, str, ...)
	return checka(sprintf("%s.byte('%s',%s)",ctype,str,table.concat({...}, ",")),
		ok, unicode[ctype].byte(str, ...))
end
testbyte("string","194,176","Ä°Ö",3,4) -- the UTF-8 seq for °
testbyte("ascii","194,176","Ä°Ö",3,4)
testbyte("utf8","176,214","Ä°Ö",2,3) -- code points for °,Ö
testbyte("utf8","65,776","\204\136A\204\136O\204\136",2,3) -- decomposed


local function testchar (ctype, ok, ...)
	return check(sprintf("%s.char(%s)",ctype,table.concat({...}, ",")),
		ok, unicode[ctype].char(...))
end
testchar("ascii", "AB", 65,66)
testchar("ascii", "\176", 176)
testchar("utf8", "\194\176", 176)


local function testcase (ctype,str,up,lo)
	check(sprintf("%s.lower('%s')", ctype, str), lo, unicode[ctype].lower(str))
	check(sprintf("%s.upper('%s')", ctype, str), up, unicode[ctype].upper(str))
end
testcase("utf8","Abäüo\204\136","ABÄÜO\204\136","abäüo\204\136")
testcase("ascii","Abäüo\204\136","ABäüO\204\136","abäüo\204\136")


local function testrev (ctype,ok,str)
	return check(sprintf("%s.reverse('%s')",ctype,str),
		ok, unicode[ctype].reverse(str))
end
testrev("ascii","b\136\204oa\176\194ba","ab°ao\204\136b");
testrev("utf8","b\204\136oa°ba","ab°ao\204\136b");



local function testfind (ctype,ok,str,pat)
	return checka(sprintf("%s.find('%s','%s')",ctype,str,pat),
		ok, unicode[ctype].find(str, pat))
end
testfind("ascii","1,1","e=mc2","%a")
testfind("ascii","3,4","e=mc2","%a%a")
testfind("ascii","5,5","e=mc2","%d")
testfind("ascii","","Ä","%a")
testfind("ascii","1,2","Ä","%A*")
testfind("utf8","1,2","Ä","%a")
testfind("utf8","1,1","o\204\136","%a*")
testfind("utf8","2,3","o\204\136","%A")
testfind("utf8","1,1","o\204\136",".")
testfind("utf8","4,5","ÜHÄPPY","[À-Ö]")
testfind("utf8","4,5","ÜHÄPPY","[Ä-]")
testfind("utf8","7,7","ÜHÄP-PY","[ä-]")
testfind("ascii","1,4","abcdef","%a*d")
testfind("utf8","1,10","äöüßü","%a*ü")
testfind("utf8","1,6","äöüß","%a*ü")
testfind("utf8","4,5,Ä","ÜHÄPPY","([À-Ö])")
testfind("utf8","1,5,ÜHÄ","ÜHÄ_PPY","([%w]+)")
testfind("utf8","1,9,ÜHÄ_PPY","ÜHÄ_PPY","([%w_]+)")


local function testgsub (ctype,ok,str,pat,repl)
	return check(sprintf("%s.gsub('%s','%s','%s')",ctype,str,pat,repl),
		ok, unicode[ctype].gsub(str,pat,repl))
end
testgsub("ascii","hello hello world world","hello world", "(%w+)", "%1 %1")
testgsub("ascii","world hello Lua from",
	"hello world from Lua", "(%w+)%s*(%w+)", "%2 %1")
testgsub("ascii","l helö wöfr rldöL müä",
	"hellö wörld fröm Lüä", "(%w+)%s*(%w+)", "%2 %1")
testgsub("utf8","wörld hellö Lüä fröm",
	"hellö wörld fröm Lüä", "(%w+)%s*(%w+)", "%2 %1")
testgsub("utf8","HÜppÄ","HÄppÜ","([À-Ö])(%l*)(%u)","%3%2%1")


fail = 0
for i=1,1114111 do
  if utf8.validate(i) then
    if i ~= utf8.byte(utf8.char(i)) then
--       error(string.format("%s", i))
      fail=fail+1
    end
  end
end
check("code-decode failures", 0, fail)

--[[ print the table
for i=192,65535,64 do
	local k = i/64
	io.write(sprintf("%04x\\%3d\\%3d ",i, 224+k/64, 128+math.mod(k,64)))
	for j=i,i+63 do
		io.write(utf8.char(j))
	end
	io.write("\n")
end
]]

