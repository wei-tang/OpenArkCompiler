var $iconst1 <[4] [4] i32> = [ [1007, 707, -273, 75], [0113, 0x4b, 75u, 75l], [75ul, 75lu, 31425926, 60223], [60223, 1619, 30, 314159]]
var $fconst1 <[4] [4] f64> = [ [1007.0, 707.0, -273.0, 75.0], [113.1, 4.0, 75.0, 75.1], [75.0, 75.1, 3.1425926, 6.02e23], [6.02e+23, 1.6e-19, 3.0, 3.14159]]
func &printf (var %p1 <* i8>)void
func &main ( ) i32 {
   call &printf (addrof a32 $fconst1)
   return (constval i32 0) }



 # EXEC: %irbuild Main.mpl
 # EXEC: %irbuild Main.irb.mpl
 # EXEC: %cmp Main.irb.mpl Main.irb.irb.mpl
