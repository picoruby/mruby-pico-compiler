class MasgnTest < PicoRubyTest
  desc "basic case 1"
  assert_equal(<<~RUBY, "1\n2")
    a, b = 1, 2
    p a, b
  RUBY
end
