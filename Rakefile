MRUBY_CONFIG=File.expand_path(ENV["MRUBY_CONFIG"] || "build_config/host.rb")
MRUBY_VERSION=ENV["MRUBY_VERSION"] || "3.0.0"

file :mruby do
  sh "git clone --depth=1 https://github.com/mruby/mruby.git"
  if MRUBY_VERSION != 'master'
    Dir.chdir 'mruby' do
      sh "git fetch --tags"
      rev = %x{git rev-parse #{MRUBY_VERSION}}
      sh "git checkout #{rev}"
    end
  end
end

desc "compile binary"
task :compile => :mruby do
  sh "cd mruby && rake all MRUBY_CONFIG=#{MRUBY_CONFIG}"
end

desc "test"
task :test => :mruby do
  sh "cd mruby && rake all test MRUBY_CONFIG=#{MRUBY_CONFIG}"
end

desc "cleanup"
task :clean do
  rm_rf "include/atom_helper.h"
  rm_rf "include/keyword_helper.h"
  rm_rf "include/token_helper.h"
  rm_rf "include/tokenizer_helper.h"
  rm_rf "include/parse.h"
  rm_rf "lib/lemon"
  rm_rf "src/parse.out"
  rm_rf "src/parse.c"
  rm_rf "src/parse.h"
  exit 0 unless File.directory?('mruby')
  sh "cd mruby && rake deep_clean"
  rm_rf "mruby/bin/picorbc"
end

task :default => :compile
