class GenValuesTest < PicoRubyTest

  if @@vm_select == :mruby

    desc "gen_values() in generator.c"
    assert_equal(<<~RUBY, "1\n[2, 3]\n{:a=>0}\n4\nhello")
      a = ->(m,*rest,m2,**opts,&block) do
        p m,rest,opts,m2; block.call
      end
      a.call(1,2,3,4,{a: 0}) {puts :hello}
    RUBY

  end

end
