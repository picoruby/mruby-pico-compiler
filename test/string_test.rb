class StringTest < PicoRubyTest
  desc "Keyword in str"
  assert_equal(<<~RUBY, "nil")
    puts "nil"
  RUBY

  desc "%q"
  assert_equal(<<~'RUBY', "a!b")
    puts %q!a\!b!
  RUBY

  desc "%Q"
  assert_equal(<<~'RUBY', "a!9b")
    puts %Q!a\!#{3 ** 2}b!
  RUBY
end
