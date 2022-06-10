class UnaryTest < PicoRubyTest
  desc "p -1"
  assert_equal(<<~RUBY, "-1")
    p -1
  RUBY

  desc "after **(pow)"
  assert_equal(<<~RUBY, @@vm_select == :mrubyc ? "0" : "0.01")
    p 10**-2
  RUBY

  desc "-1**2"
  assert_equal(<<~RUBY, "-1")
    p -1**2
  RUBY

  desc "-1.3**3"
  assert_equal(<<~RUBY, "-2.197")
    p -1.3**3
  RUBY
end
