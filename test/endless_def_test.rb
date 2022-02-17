class EndlessDefTest < PicoRubyTest
  desc "endless def arg"
  assert_equal(<<~RUBY, "55")
    def fib(n) = n < 2 ? n : fib(n-1) + fib(n-2)
    p fib(10)
  RUBY

  desc "endless def command"
  assert_equal(<<~RUBY, "you!")
    def hey(word) = puts word
    hey "you!"
  RUBY
end
