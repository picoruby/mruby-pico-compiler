class RescueTest < PicoRubyTest

  desc "simple rescue"
  assert_equal(<<~RUBY, "true")
    begin
      raise
    rescue
      p true
    end
  RUBY

  desc "simple else"
  assert_equal(<<~RUBY, "false")
    begin
      0
    rescue
      p true
    else
      p false
    end
  RUBY

  desc "simple ensure"
  assert_equal(<<~RUBY, "true\nnil")
    begin
      raise
    rescue
      p true
    else
      p false
    ensure
      p nil
    end
  RUBY

  desc "rescue error class"
  assert_equal(<<~RUBY, "RuntimeError\nMy Error")
    begin
      raise "My Error"
    rescue => e
      puts e.class
      puts e.message
    end
  RUBY

  desc "retry"
  assert_equal(<<~RUBY, "2")
    def my_method
      a = 0
      begin
        raise if a < 2
      rescue => e
        a += 1
        retry
      end
      p a
    end
    my_method
  RUBY

  if @@vm_select == :mruby

    desc "splat error class"
    assert_equal(<<~RUBY, "catched")
      errors = [TypeError]
      begin
        raise TypeError
      rescue *errors
        puts "catched"
      end
    RUBY

    desc "splat error class 2"
    assert_equal(<<~RUBY, "catched")
      errors = [TypeError]
      begin
        raise TypeError
      rescue StandardError, *errors
        puts "catched"
      end
    RUBY

    desc "rescue NoMethodError"
    assert_equal(<<~RUBY, "Yes")
      begin
        method_does_not_exist
      rescue ArgumentError => e
        puts "This should not happen 1"
      rescue NoMethodError => e
        puts "Yes"
      rescue => e
        puts "This should not happen 2"
      end
    RUBY

  end

end
