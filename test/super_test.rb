class SuperTest < PicoRubyTest

  desc "super 1"
  assert_equal(<<~'RUBY', "1\n2")
    class A
      def a(v=0)
        1 + v
      end
    end
    class B < A
      def a
        super
      end
    end
    class C < A
      def a
        super(1)
      end
    end
    p B.new.a
    p C.new.a
  RUBY

end
