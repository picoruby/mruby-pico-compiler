class ModuleTest < PicoRubyTest

  if @@vm_select == :mruby

    desc "include a Module"
    assert_equal(<<~RUBY, "0")
      module A
        def a
          p 0
        end
      end
      include A
      self.a
    RUBY
  end

end

