class KargTest < PicoRubyTest
  desc "def method with karg"
  assert_equal(<<~RUBY, "1\n2")
    def m1(k: 1)
      puts k
    end
    m1
    m1(k:2)
  RUBY

  desc "required keyword"
  assert_equal(<<~RUBY, "true")
    def m2(k:)
      puts k
    end
    m2(k: true)
  RUBY

  desc "complicated case"
  assert_equal(<<~RUBY, "13")
    def m3(a, b, o1 = 1, o2 = 2, *c, k1:, k2: 3, k3:)
      puts k1 + k2 + k3
    end
    m3("dummy", "dummy", k3: 1, k2: 2, k1: 10)
  RUBY
end

