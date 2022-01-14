MRuby::Build.new do |conf|
  toolchain :gcc
  conf.gem File.expand_path('../../', __FILE__)
  conf.enable_test

  conf.gem core: 'mruby-io'

  if ENV['DEBUG'] == 'true'
    conf.enable_debug
    conf.cc.defines = %w(MRB_ENABLE_DEBUG_HOOK)
    conf.gem core: 'mruby-bin-debugger'
  else
    conf.cc.defines = %w(NDEBUG)
  end
  conf.cc.defines << "MRBC_ALLOC_LIBC"
  conf.cc.defines << "REGEX_USE_ALLOC_LIBC"
end
