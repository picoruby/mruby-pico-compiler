class ScallTest < PicoRubyTest

  desc "&. oprator"
  assert_equal(<<~RUBY, "0\nnil\nfalse\nfalse")
    def a(v)
      case v
      when 0
        nil
      when 1
        true
      else
        false
      end
    end
    p a(0).to_i
    p a(0)&.to_i
    puts a(2).to_s
    puts a(2)&.to_s
  RUBY

  desc "&. oprator with block"
  assert_equal(<<~RUBY, "0\n1")
    [0,1]&.each do |i|
      puts i
    end
  RUBY
end
