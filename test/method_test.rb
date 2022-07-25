class MethodTest < PicoRubyTest
  desc "Mysterious case. Differnt from `p()` *space*"
  assert_equal(<<~RUBY, "nil")
    p ()
  RUBY

  desc "Another *space* case"
  assert_equal(<<~RUBY, "1")
    p (1) do end
  RUBY

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
