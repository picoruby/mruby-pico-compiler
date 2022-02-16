class LambdaTest < PicoRubyTest

  if @@vm_select == :mruby
    desc "lambda call"
    assert_equal(<<~RUBY, "1\n2\n[3]\n4")
      a = -> (m,o=true,*rest,m2) { p m,o,rest,m2 }
      a.call(1,2,3,4)
    RUBY
  end

end
