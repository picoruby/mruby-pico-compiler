class UnaryTest < PicoRubyTest
  desc "after **(pow)"
  assert_equal(<<~RUBY, @@vm_select == :mrubyc ? "0" : "0.01")
    p 10**-2
  RUBY
end
