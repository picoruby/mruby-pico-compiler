class RightwardAssignTest < PicoRubyTest
  assert_equal(<<~RUBY, "hello")
    :hello.to_s => a
    puts a
  RUBY
end
