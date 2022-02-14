class RestArgTest < PicoRubyTest

  desc "mruby/c should handle restarg"
  assert_equal(<<~RUBY, "1\n[2, 3, 4]")
    def a(m, *rest)
      p m, rest
    end
    a(1,2,3,4)
  RUBY

  desc "restarg in block"
  assert_equal(<<~RUBY, "[0]\n[1]")
    [0,1].each{|*v| p v}
  RUBY

  if @@vm_select == :mruby
    desc "restarg in mruby (mruby/c doesn't support m2 args)"
    assert_equal(<<~RUBY, "1\n[2, 3]\n4")
      def a(m, *rest, m2)
        p m, rest, m2
      end
      a(1,2,3,4)
    RUBY
  end

end
