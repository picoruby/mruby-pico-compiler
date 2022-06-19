class MasgnTest < PicoRubyTest
  desc "basic case 1"
  assert_equal(<<~RUBY, "1\n2")
    a, b = 1, 2
    p a, b
  RUBY

  desc "basic case 2"
  assert_equal(<<~RUBY, "1\n2")
    a, b = [1, 2]
    p a, b
  RUBY

  desc "RHS has only one item"
  assert_equal(<<~RUBY, "1\nnil")
    a, b = 1
    p a, b
  RUBY

  desc "baz should be nil"
  assert_equal(<<~RUBY, "1\n2\nnil")
    baz = true
    foo, bar, baz = 1, 2
    p foo, bar, baz
  RUBY

  desc "3 should be ignored"
  assert_equal(<<~RUBY, "1\n2")
    foo, bar = 1, 2, 3
    p foo, bar
  RUBY

  desc "single pre"
  assert_equal(<<~RUBY, "[1, 2, 3]")
    foo      = 1, 2, 3
    p foo
  RUBY

  desc "single rest"
  assert_equal(<<~RUBY, "[1, 2, 3]")
    *foo     = 1, 2, 3
    p foo
  RUBY

  desc "rest gets the rest"
  assert_equal(<<~RUBY, "1\n[2, 3]")
    foo,*bar = 1, 2, 3
    p foo, bar
  RUBY

  desc "post should be nil"
  assert_equal(<<~RUBY, "nil")
    post = true
    pre1, pre2, *rest, post = 1, 2
    p post
  RUBY

  # TODO
  #desc "post should be nil"
  #assert_equal(<<~RUBY, "1\n2\n3")
  #  (foo, bar), baz = [1, 2], 3
  #  p foo, bar, baz
  #RUBY

  desc "complecated case"
  assert_equal(<<~'RUBY', "1\n[\"a\", \"b\", 2]")
    class C
      attr_accessor :foo, :bar
      def foo=( v )
        @foo = v
      end
      def []=(i,v)
        @bar = ["a", "b", "c"]
        @bar[i] = v
      end
    end
    obj = C.new
    obj.foo, obj[2] = 1, 2
    p obj.foo, obj.bar
  RUBY

  if @@vm_select == :mruby
    desc "RHS has only one item, LHS has a rest and a post"
    assert_equal(<<~RUBY, "1\nnil\n[]\nnil")
      a, b, *c, d = 1
      p a, b, c, d
    RUBY

    desc "RHS has only one item which is an array, LHS has a rest and a post"
    assert_equal(<<~RUBY, "0\n[]\n1")
      ary = [0, 1]
      a, *b, c = ary
      p a, b, c
    RUBY

    desc "splat in RHS"
    assert_equal(<<~RUBY, "1\n[9, 8]\n3\n4")
      ary = [9, 8]
      a,*b,c,d=1,*ary,3,4
      p a,b,c,d
    RUBY
  end
end
