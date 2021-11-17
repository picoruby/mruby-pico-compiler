##
## PicoCompiler Test
##

assert("PicoCompiler#hello") do
  t = PicoCompiler.new "hello"
  assert_equal("hello", t.hello)
end

assert("PicoCompiler#bye") do
  t = PicoCompiler.new "hello"
  assert_equal("hello bye", t.bye)
end

assert("PicoCompiler.hi") do
  assert_equal("hi!!", PicoCompiler.hi)
end
