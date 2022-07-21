class DstrTest < PicoRubyTest
  desc "interpolation 1"
  assert_equal(<<~'RUBY', 'hello Ruby')
    @ivar = "Ruby"
    puts "hello #{@ivar}"
  RUBY

  desc "interpolation 2"
  assert_equal(<<~'RUBY', '2a')
    puts "#{1+1}" + "a"
  RUBY

  desc "interpolation 3"
  assert_equal(<<~'RUBY', '_101ab')
    i = 100
    puts "_" + "#{i+1}a" + "b"
  RUBY
end
