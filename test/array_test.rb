class ArrayTest < PicoRubyTest
  desc "array basic case"
  assert_equal(<<~RUBY, "[1, 2, 3, [4, 5, 6, 7, 8], 9, 10]")
    p([1,2,3,[4,5,6,7,8],9,10])
  RUBY

  desc "PICORUBY_ARRAY_SPLIT_COUNT case"
  assert_equal(<<~RUBY, ":A63\n:A64\n:A65\n:A66\n70")
    ary = %i(
      A00 A01 A02 A03 A04 A05 A06 A07 A08 A09
      A10 A11 A12 A13 A14 A15 A16 A17 A18 A19
      A20 A21 A22 A23 A24 A25 A26 A27 A28 A29
      A30 A31 A32 A33 A34 A35 A36 A37 A38 A39
      A40 A41 A42 A43 A44 A45 A46 A47 A48 A49
      A50 A51 A52 A53 A54 A55 A56 A57 A58 A59
      A60 A61 A62 A63 A64 A65 A66 A67 A68 A69
    )
    p ary[63]
    p ary[64]
    p ary[65]
    p ary[66]
    puts ary.size
  RUBY

  desc "getter [x,y]"
  assert_equal(<<~RUBY, "[2, 3]")
    ary = [0,1,2,3]
    p ary[2, 3]
  RUBY

  desc "setter [x,y]"
  assert_equal(<<~RUBY, "[0, 1, 2, 3, 44]")
    ary = [0,1,2,3]
    ary[4, 10] = 44
    p ary
  RUBY
end
