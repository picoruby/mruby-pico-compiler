#! /usr/bin/env ruby

require 'fileutils'
require 'rake/file_list'
require "tempfile"

class PicoRubyTest
  def initialize
    @@pending = false
    @@exit_code = 0
    @@success_count = 0
    @@pending_count = 0
    @@brown = "\e[33m"
    @@green = "\e[32;1m"
    @@red = "\e[31;1m"
    @@reset = "\e[m"
    @@failure_struct = Struct.new(:filename, :description, :script, :expected, :actual)
    @@failures = []
    @@description = ""
    @@mruby_path = ENV['MRUBY_COMMAND']
    @@picorbc_path = ENV['PICORBC_COMMAND']
    @@vm_select = ENV['USE_MRUBY'] ? :mruby : :mrubyc
    puts
    puts <<~"PREFACE"
      Virtual machine: #{@@vm_select}
      mruby_path:      #{@@mruby_path}
      picorbc_path:    #{@@picorbc_path}
    PREFACE
    puts
  end

  def exit_code
    @@exit_code
  end

  def test
    puts 'Start test'
    Dir.glob(File.expand_path('../../*_test.rb', __FILE__)).each do |file|
      @@filename = File.basename(file)
      load file
      @@pending = false
    end
    puts
    puts "Summary:"
    print @@green
    puts "  Success: #{@@success_count}"
    print @@brown if @@pending_count > 0
    puts "  Pending: #{@@pending_count}#{@@reset}"
    if @@failures.count > 0
      print @@red
    else
      print @@green
    end
    puts "  Failure: #{@@failures.count}"
    @@failures.each do |failure|
      puts "    File: #{failure.filename}"
      puts "      Description: #{failure.description}"
      puts "      Script:"
      puts "        #{failure.script.gsub(/\n/, %Q/\n        /)}"
      puts "      Expected:"
      puts "        #{failure.expected.gsub(/\n/, %Q/\n        /)}"
      puts "      Actual:"
      puts "        #{failure.actual.gsub(/\n/, %Q/\n        /)}"
    end
    print @@reset
  end

  def self.desc(text)
    @@description = text
  end

  def self.assert_equal(script, expected)
    if @@pending
      print "#{@@brown}.#{@@reset}"
      @@pending_count += 1
      return
    end
    if @@vm_select == :mruby
      rbfile = String.new
      mrbfile = String.new
      actual = nil
      Tempfile.open do |f|
        f.puts script
        rbfile = f.path
      end
      Tempfile.open do |f|
        mrbfile = f.path
        # For GitHub Actions
        FileUtils.chmod 0755, rbfile
        FileUtils.chmod 0755, mrbfile
        `#{@@picorbc_path} #{rbfile} -o #{mrbfile}`
      end
      actual = `#{@@mruby_path} -b '#{mrbfile}'`.chomp.gsub(/\r/, "")
    else
      actual = `#{@@mruby_path} -e '#{script}'`.chomp.gsub(/\r/, "")
    end
    if actual == expected
      print "#{@@green}."
      @@success_count += 1
    else
      print "#{@@red}."
      @@failures << @@failure_struct.new(@@filename, @@description, script.chomp, expected, actual)
      @@exit_code = 1
    end
    print @@reset
  end

  def self.pending
    @@pending = true
  end
end

picoruby_test = PicoRubyTest.new
picoruby_test.test

exit picoruby_test.exit_code
