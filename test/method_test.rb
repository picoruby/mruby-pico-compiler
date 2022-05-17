class MethodTest < PicoRubyTest
  desc "Integer class"
  assert_equal(<<~RUBY, "1234")
    puts 1234.to_s
  RUBY

  desc "Method chain"
  assert_equal(<<~RUBY, "1234")
    puts 1234.to_s.to_i
  RUBY

  desc "SCALL"
  assert_equal(<<~RUBY, "1")
    def my_block(&b)
      @cb = b
    end
    self.my_block do |v|
      p v
    end
    @cb&.call(1)
  RUBY
end
