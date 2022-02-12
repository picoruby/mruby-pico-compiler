class SplatTest < PicoRubyTest

  if @@vm_select == :mruby

    desc "splat in fcall 1"
    assert_equal(<<~RUBY, "1")
      p(*1)
    RUBY

    desc "splat in fcall 2"
    assert_equal(<<~RUBY, "1\n2")
      p(1, *2)
    RUBY

    desc "splat in fcall 3"
    assert_equal(<<~RUBY, "[3, 4]\n1\n2\n[3, 4]")
      p(*1,2,p([3,4]))
    RUBY

    desc "splat in fcall 4"
    assert_equal(<<~RUBY, "3\n4\n1\n2\n[3, 4]")
      p(*1,2,p(*[3,4]))
    RUBY

    desc "splat in fcall 5"
    assert_equal(<<~RUBY, "[\"str\"]")
      a = *"str"
      p a
    RUBY

    desc "splat in fcall 6"
    assert_equal(<<~RUBY, "1\n2\n3")
      p(1,2,*3)
    RUBY

    desc "splat in fcall 7"
    assert_equal(<<~RUBY, "hey")
      case 0
      when *[0]
        puts "hey"
      end
    RUBY

  end

end

