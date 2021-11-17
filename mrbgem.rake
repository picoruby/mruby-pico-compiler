MRuby::Gem::Specification.new('mruby-pico-compiler') do |spec|
  spec.bins = ['picorbc']
  spec.license = 'MIT'
  spec.authors = 'HASUMI Hitoshi'

  include_dir = "#{build_dir}/include"
  directory include_dir
  spec.cc.include_paths << include_dir

  objs = %w[parse].map do |name|
    src = "#{dir}/src/#{name}.c"
    if build.cxx_exception_enabled?
      build.compile_as_cxx(src)
    else
      objfile(src.pathmap("#{build_dir}/src/%n"))
    end
  end
  build.libmruby_core_objs << objs

  file "#{include_dir}/atom_helper.h" => "#{dir}/src/parse_header.h" do |t|
    File.open(t.name, "w") do |file|
      file.puts <<~TEXT
        inline static char *atom_name(AtomType n)
        {
          switch(n) {
      TEXT
      File.open("parse_header.h", "r") do |f|
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

  file "#{include_dir}/ptr_size.h" => ["#{dir}/lib/ptr_size_generator", include_dir] do
    sh "cd #{dir}/lib && ./ptr_size_generator && mv ptr_size.h #{include_dir}"
  end

  file "#{dir}/lib/ptr_size_generator" => "#{dir}/lib/ptr_size_generator.c" do
    sh "cd #{dir}/lib && cc -static -o ptr_size_generator ptr_size_generator.c"
  end

  file "#{dir}/src/parse.c" => %W(#{include_dir}/atom_helper.h #{dir}/src/parse.y #{dir}/lib/lemon #{include_dir}/ptr_size.h) do
    require "open3"
    cmd = "cd #{dir}/src && #{dir}/lib/lemon -p #{ENV['LEMON_MACRO']} ./parse.y"
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

  file "#{dir}/lib/lemon" => %W(#{dir}/lib/lemon.c) do
    sh "cd #{dir}/lib && cc -O0 -g3 -Wall -Wpointer-arith -std=gnu99 -o lemon lemon.c"
  end
end
