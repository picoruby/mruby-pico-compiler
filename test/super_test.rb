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

  desc "super with block"
  assert_equal(<<~RUBY, "1\n2\n4")
    class A
      def a(v, &b)
        yield(v)
      end
    end
    A.new.a(1){|v| p v}
    class B < A
      def a(v, &b)
        super
      end
    end
    B.new.a(2){|v| p v}
    class C < A
      def a(v, &b)
        super(v) do |v|
          p v + 1
        end
      end
    end
    C.new.a(3)
  RUBY

end
