class NamespaceTest < PicoRubyTest

  if @@vm_select == :mruby

    desc "name space 1"
    assert_equal(<<~RUBY, ":foo")
      module A
        module B
          FOO = :foo
        end
      end
      p A::B::FOO
    RUBY

    desc "name space 2"
    assert_equal(<<~RUBY, ":foo")
      module A
      end
      module A::B
        FOO = :foo
      end
      p A::B::FOO
    RUBY

    desc "name space 3"
    assert_equal(<<~RUBY, ":foo")
      module A
        class B
          FOO = :foo
        end
      end
      p A::B::FOO
    RUBY

    desc "name space fail"
    assert_equal(<<~RUBY, "NameError")
      module A
        module B
          FOO = :foo
        end
      end
      begin
        A::FOO
      rescue => e
        puts e.class
      end
    RUBY

  end
end
