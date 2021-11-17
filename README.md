# mruby-pico-compiler   [![build](https://github.com/hasumikin/mruby-pico-compiler/actions/workflows/ci.yml/badge.svg)](https://github.com/hasumikin/mruby-pico-compiler/actions/workflows/ci.yml)
PicoCompiler class
## install by mrbgems
- add conf.gem line to `build_config.rb`

```ruby
MRuby::Build.new do |conf|

    # ... (snip) ...

    conf.gem :github => 'hasumikin/mruby-pico-compiler'
end
```
## example
```ruby
p PicoCompiler.hi
#=> "hi!!"
t = PicoCompiler.new "hello"
p t.hello
#=> "hello"
p t.bye
#=> "hello bye"
```

## License
under the MIT License:
- see LICENSE file
