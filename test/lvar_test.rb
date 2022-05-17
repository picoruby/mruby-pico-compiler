class LvarTest < PicoRubyTest
  desc "Implicit initialize"
  assert_equal(<<~RUBY, 'nil')
    p(a=a)
  RUBY
end
