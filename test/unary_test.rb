class UnaryTest < PicoRubyTest
  desc "after **(pow)"
  assert_equal(<<~RUBY, ENV['PICORUBY'] ? "0" : "0.01")
    p 10**-2
  RUBY
end
