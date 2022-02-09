class UnaryTest < PicoRubyTest
  desc "after **(pow)"
  assert_equal(<<~RUBY, "0") # mruby should return 0.01 though
    p 10**-2
  RUBY
end
