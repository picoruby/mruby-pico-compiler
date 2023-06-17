class ClassTest < PicoRubyTest
  desc "re-define Array#collect"
  assert_equal(<<~RUBY, "[\"1\", \"2\"]")
    def collect
      i = 0
      ary = []
      while i < length
        ary[i] = yield self[i]
        i += 1
      end
      return ary
    end
    p [1, 2].collect {|e| e.to_s }
  RUBY

  desc "inheritance from nested class"
  assert_equal(<<~RUBY, "4\n6")
    class BLE
      class AttServer
        def get_packet(i)
          puts i * 2
        end
      end
    end
    class MyServer < BLE::AttServer
      def get_packet(i)
        puts i * 3
      end
    end
    BLE::AttServer.new.get_packet(2)
    MyServer.new.get_packet(2)
  RUBY
end
