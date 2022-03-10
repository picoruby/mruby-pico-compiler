class FloatTest < PicoRubyTest
  desc "plus"
  assert_equal(<<~RUBY, "1.1")
    p 1.1
  RUBY

  desc "minus"
  assert_equal(<<~RUBY, "-1.1")
    p -1.1
  RUBY

end
