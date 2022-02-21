class KeywordTest < PicoRubyTest
  desc "a keyword can be a method name"
  assert_equal(<<~RUBY, 'OK')
    def next(v)
      puts v
    end
    self.next("OK")
  RUBY
end
