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

  desc "null letter strip!"
  assert_equal(<<~'RUBY', "")
    s = "\0"
    puts s.strip!
  RUBY

  desc "getter [x, y]"
  assert_equal(<<~'RUBY', "")
    s = "bar"
    puts s[2, 0]
  RUBY

  desc "binary"
  assert_equal(<<~'RUBY', "1")
    p "\x00".size
  RUBY

end
