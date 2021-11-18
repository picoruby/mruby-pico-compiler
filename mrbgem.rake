MRuby::Gem::Specification.new('mruby-pico-compiler') do |spec|
  spec.bins = ['picorbc']
  spec.license = 'MIT'
  spec.authors = 'HASUMI Hitoshi'

  include_dir = "#{build_dir}/include"
  src_dir = "#{dir}/src"
  lib_dir = "#{dir}/lib"
  spec.cc.include_paths << include_dir

  objs = %w[parse].map do |name|
    src = "#{src_dir}/#{name}.c"
    if build.cxx_exception_enabled?
      build.compile_as_cxx(src)
    else
      objfile(src.pathmap("#{build_dir}/src/%n"))
    end
  end
  build.libmruby_core_objs << objs

  directory include_dir

  file "#{include_dir}/parse.h" => [include_dir, "#{src_dir}/parse.h"] do |t|
    sh "cp #{src_dir}/parse.h #{t.name}"
  end

  file "#{include_dir}/keyword_helper.h" => [include_dir, "#{include_dir}/parse.h"] do |t|
    File.open(t.name, "w") do |file|
      file.puts <<~TEXT
        #include "parse.h"
        int8_t
        keyword(const char *word)
        {
      TEXT
      File.open("#{include_dir}/parse.h", "r") do |f|
        f.each_line do |line|
          data = line.match(/\A#define\s+KW_(\w+)\s+\d+$/)
          if data && !data[1].match?('modifier_') && !%w(do_cond do_block).include?(data[1])
            file.puts "  if (!strcmp(word, \"#{data[1]}\")) { return KW_#{data[1]}; } else"
          end
        end
      end
      file.puts <<~TEXT
          { return -1; }
        }
      TEXT
    end
  end

  file "#{include_dir}/token_helper.h" => [include_dir, "#{src_dir}/parse.h"] do |t|
    File.open(t.name, "w") do |file|
      file.puts <<~TEXT
        inline static const char *token_name(int n)
        {
          switch(n) {
      TEXT
      File.open("#{src_dir}/parse.h", "r") do |f|
        f.each_line do |line|
          data = line.match(/\A#define\s+(\w+)\s+\d+$/)
          if data
            file.puts "    case(#{data[1]}): return \"#{data[1].ljust(12)}\";"
          end
        end
      end
      file.puts <<~TEXT
            default: return "\\e[37;41;1m\\\"UNDEFINED\\\"\\e[m";
          }
        }
      TEXT
    end
  end

  file "#{include_dir}/atom_helper.h" => [include_dir, "#{src_dir}/parse_header.h"] do |t|
    File.open(t.name, "w") do |file|
      file.puts <<~TEXT
        inline static const char *atom_name(AtomType n)
        {
          switch(n) {
      TEXT
      File.open("#{src_dir}/parse_header.h", "r") do |f|
        f.each_line do |line|
          break if line.match?("enum atom_type")
        end
        f.each_line do |line|
          break if line.match?("AtomType")
          data = line.match(/(\w+)(\s+=\s+\d+)?,?$/)
          if data
            file.puts "    case(#{data[1]}): return \"#{data[1]}\";"
          end
        end
      end
      file.puts <<~TEXT
            default: return "\\e[37;41;1m\\\"UNDEFINED\\\"\\e[m";
          }
        }
      TEXT
    end
  end

  file "#{include_dir}/tokenizer_helper.h" => [include_dir, "#{src_dir}/tokenizer.h", "#{src_dir}/token.h"] do |t|
    File.open(t.name, "w") do |file|
      file.puts <<~TEXT
        inline static const char *tokenizer_state_name(State n)
        {
          switch(n) {
      TEXT
      File.open("#{src_dir}/token.h", "r") do |f|
        f.each_line do |line|
          break if line.match?("enum state")
        end
        f.each_line do |line|
          break if line.match?("State")
          data = line.match(/\A\s+(\w+)\s+/)
          if data
            file.puts "    case(#{data[1]}):#{' '*(13 - data[1].length)} return \"#{data[1].ljust(12)}\";"
          end
        end
      end
      file.puts <<~TEXT
            default:
              return "\\e[37;41;1mCOMBINED VALUE\\e[m";
          }
        }
      TEXT
      file.puts ""
      file.puts <<~TEXT
        inline static const char *tokenizer_mode_name(Mode n)
        {
          switch(n) {
      TEXT
      File.open("#{src_dir}/tokenizer.h", "r") do |f|
        f.each_line do |line|
          break if line.match?("enum mode")
        end
        f.each_line do |line|
          break if line.match?("Mode")
          data = line.match(/\A\s+(\w+)/)
          if data
            file.puts "    case(#{data[1]}):#{' '*(20 - data[1].length)} return \"#{data[1].rjust(19)}\";"
          end
        end
      end
      file.puts <<~TEXT
            default: return "\\e[37;41;1m\\\"UNDEFINED         \\\"\\e[m";
          }
        }
      TEXT
    end
  end

  file "#{include_dir}/ptr_size.h" => ["#{lib_dir}/ptr_size_generator", include_dir] do
    sh "cd #{lib_dir} && ./ptr_size_generator && mv ptr_size.h #{include_dir}"
  end

  file "#{lib_dir}/ptr_size_generator" => "#{lib_dir}/ptr_size_generator.c" do
    sh "cd #{lib_dir} && cc -static -o ptr_size_generator ptr_size_generator.c"
  end

  file "#{src_dir}/parse.h" => "#{src_dir}/parse.c" do
  end

  file "#{src_dir}/compiler.c" => %W(#{include_dir}/token_helper.h
                                     #{include_dir}/tokenizer_helper.h) do
  end

  file "#{src_dir}/tokenizer.c" => %W(#{include_dir}/token_helper.h
                                      #{include_dir}/keyword_helper.h) do
  end

  file "#{src_dir}/parse.c" => %W(#{include_dir}
                                  #{include_dir}/atom_helper.h
                                  #{src_dir}/parse.y
                                  #{lib_dir}/lemon
                                  #{include_dir}/ptr_size.h) do
    require "open3"
    cmd = "cd #{src_dir} && #{lib_dir}/lemon -p #{ENV['LEMON_MACRO']} ./parse.y"
    puts cmd
    out, err = Open3.capture3(cmd)
    puts out
    err.split("\n").each do |e|
      puts "\e[33;41;1m#{e}\e[m"
      unless e.include?("parsing conflict")
        raise "Parsing conflict"
      end
    end
  end

  file "#{lib_dir}/lemon" => %W(#{lib_dir}/lemon.c) do
    sh "cd #{lib_dir} && cc -O0 -g3 -Wall -Wpointer-arith -std=gnu99 -o lemon lemon.c"
  end
end
