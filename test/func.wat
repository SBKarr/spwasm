;; Test `func` declarations, i.e. functions

(module
  ;; Auxiliary definition
  (type $sig (func))
  (func $dummy)

  ;; Syntax

  (func)
  (func (export "f"))
  (func $f)
  (func $h (export "g"))

  (func (local))
  (func (local) (local))
  (func (local i32))
  (func (local $x i32))
  (func (local i32 f64 i64))
  (func (local i32) (local f64))
  (func (local i32 f32) (local $x i64) (local) (local i32 f64))

  (func (param))
  (func (param) (param))
  (func (param i32))
  (func (param $x i32))
  (func (param i32 f64 i64))
  (func (param i32) (param f64))
  (func (param i32 f32) (param $x i64) (param) (param i32 f64))

  (func (result i32) (unreachable))

  (type $sig-1 (func))
  (type $sig-2 (func (result i32)))
  (type $sig-3 (func (param $x i32)))
  (type $sig-4 (func (param i32 f64 i32) (result i32)))

  (func (export "type-use-1") (type $sig-1))
  (func (export "type-use-2") (type $sig-2) (i32.const 0))
  (func (export "type-use-3") (type $sig-3))
  (func (export "type-use-4") (type $sig-4) (i32.const 0))
  (func (export "type-use-5") (type $sig-2) (result i32) (i32.const 0))
  (func (export "type-use-6") (type $sig-3) (param i32))
  (func (export "type-use-7")
    (type $sig-4) (param i32) (param f64 i32) (result i32) (i32.const 0)
  )

  (func (type $sig))
  (func (type $forward))  ;; forward reference

  (func $complex
    (param i32 f32) (param $x i64) (param) (param i32)
    (result) (result i32) (result)
    (local f32) (local $y i32) (local i64 i32) (local) (local f64 i32)
    (unreachable) (unreachable)
  )
  (func $complex-sig
    (type $sig)
    (local f32) (local $y i32) (local i64 i32) (local) (local f64 i32)
    (unreachable) (unreachable)
  )

  (type $forward (func))

  ;; Typing of locals

  (func (export "local-first-i32") (result i32) (local i32 i32) (get_local 0))
  (func (export "local-first-i64") (result i64) (local i64 i64) (get_local 0))
  (func (export "local-first-f32") (result f32) (local f32 f32) (get_local 0))
  (func (export "local-first-f64") (result f64) (local f64 f64) (get_local 0))
  (func (export "local-second-i32") (result i32) (local i32 i32) (get_local 1))
  (func (export "local-second-i64") (result i64) (local i64 i64) (get_local 1))
  (func (export "local-second-f32") (result f32) (local f32 f32) (get_local 1))
  (func (export "local-second-f64") (result f64) (local f64 f64) (get_local 1))
  (func (export "local-mixed") (result f64)
    (local f32) (local $x i32) (local i64 i32) (local) (local f64 i32)
    (drop (f32.neg (get_local 0)))
    (drop (i32.eqz (get_local 1)))
    (drop (i64.eqz (get_local 2)))
    (drop (i32.eqz (get_local 3)))
    (drop (f64.neg (get_local 4)))
    (drop (i32.eqz (get_local 5)))
    (get_local 4)
  )

  ;; Typing of parameters

  (func (export "param-first-i32") (param i32 i32) (result i32) (get_local 0))
  (func (export "param-first-i64") (param i64 i64) (result i64) (get_local 0))
  (func (export "param-first-f32") (param f32 f32) (result f32) (get_local 0))
  (func (export "param-first-f64") (param f64 f64) (result f64) (get_local 0))
  (func (export "param-second-i32") (param i32 i32) (result i32) (get_local 1))
  (func (export "param-second-i64") (param i64 i64) (result i64) (get_local 1))
  (func (export "param-second-f32") (param f32 f32) (result f32) (get_local 1))
  (func (export "param-second-f64") (param f64 f64) (result f64) (get_local 1))
  (func (export "param-mixed") (param f32 i32) (param) (param $x i64) (param i32 f64 i32)
    (result f64)
    (drop (f32.neg (get_local 0)))
    (drop (i32.eqz (get_local 1)))
    (drop (i64.eqz (get_local 2)))
    (drop (i32.eqz (get_local 3)))
    (drop (f64.neg (get_local 4)))
    (drop (i32.eqz (get_local 5)))
    (get_local 4)
  )

  ;; Typing of result

  (func (export "empty"))
  (func (export "value-void") (call $dummy))
  (func (export "value-i32") (result i32) (i32.const 77))
  (func (export "value-i64") (result i64) (i64.const 7777))
  (func (export "value-f32") (result f32) (f32.const 77.7))
  (func (export "value-f64") (result f64) (f64.const 77.77))
  (func (export "value-block-void") (block (call $dummy) (call $dummy)))
  (func (export "value-block-i32") (result i32)
    (block (result i32) (call $dummy) (i32.const 77))
  )

  (func (export "return-empty") (return))
  (func (export "return-i32") (result i32) (return (i32.const 78)))
  (func (export "return-i64") (result i64) (return (i64.const 7878)))
  (func (export "return-f32") (result f32) (return (f32.const 78.7)))
  (func (export "return-f64") (result f64) (return (f64.const 78.78)))
  (func (export "return-block-i32") (result i32)
    (return (block (result i32) (call $dummy) (i32.const 77)))
  )

  (func (export "break-empty") (br 0))
  (func (export "break-i32") (result i32) (br 0 (i32.const 79)))
  (func (export "break-i64") (result i64) (br 0 (i64.const 7979)))
  (func (export "break-f32") (result f32) (br 0 (f32.const 79.9)))
  (func (export "break-f64") (result f64) (br 0 (f64.const 79.79)))
  (func (export "break-block-i32") (result i32)
    (br 0 (block (result i32) (call $dummy) (i32.const 77)))
  )

  (func (export "break-br_if-empty") (param i32)
    (br_if 0 (get_local 0))
  )
  (func (export "break-br_if-num") (param i32) (result i32)
    (drop (br_if 0 (i32.const 50) (get_local 0))) (i32.const 51)
  )

  (func (export "break-br_table-empty") (param i32)
    (br_table 0 0 0 (get_local 0))
  )
  (func (export "break-br_table-num") (param i32) (result i32)
    (br_table 0 0 (i32.const 50) (get_local 0)) (i32.const 51)
  )
  (func (export "break-br_table-nested-empty") (param i32)
    (block (br_table 0 1 0 (get_local 0)))
  )
  (func (export "break-br_table-nested-num") (param i32) (result i32)
    (i32.add
      (block (result i32)
        (br_table 0 1 0 (i32.const 50) (get_local 0)) (i32.const 51)
      )
      (i32.const 2)
    )
  )

  ;; Default initialization of locals

  (func (export "init-local-i32") (result i32) (local i32) (get_local 0))
  (func (export "init-local-i64") (result i64) (local i64) (get_local 0))
  (func (export "init-local-f32") (result f32) (local f32) (get_local 0))
  (func (export "init-local-f64") (result f64) (local f64) (get_local 0))
)

;;assert_return (invoke "type-use-1"))
;;assert_return (invoke "type-use-2") (i32.const 0))
;;assert_return (invoke "type-use-3" (i32.const 1)))
;;assert_return
;;  (invoke "type-use-4" (i32.const 1) (f64.const 1) (i32.const 1))
;;  (i32.const 0)
;;)
;;assert_return (invoke "type-use-5") (i32.const 0))
;;assert_return (invoke "type-use-6" (i32.const 1)))
;;assert_return
;;  (invoke "type-use-7" (i32.const 1) (f64.const 1) (i32.const 1))
;;  (i32.const 0)
;;)

;;assert_return (invoke "local-first-i32") (i32.const 0))
;;assert_return (invoke "local-first-i64") (i64.const 0))
;;assert_return (invoke "local-first-f32") (f32.const 0))
;;assert_return (invoke "local-first-f64") (f64.const 0))
;;assert_return (invoke "local-second-i32") (i32.const 0))
;;assert_return (invoke "local-second-i64") (i64.const 0))
;;assert_return (invoke "local-second-f32") (f32.const 0))
;;assert_return (invoke "local-second-f64") (f64.const 0))
;;assert_return (invoke "local-mixed") (f64.const 0))

;;assert_return
;;  (invoke "param-first-i32" (i32.const 2) (i32.const 3)) (i32.const 2)
;;)
;;assert_return
;;  (invoke "param-first-i64" (i64.const 2) (i64.const 3)) (i64.const 2)
;;)
;;assert_return
;;  (invoke "param-first-f32" (f32.const 2) (f32.const 3)) (f32.const 2)
;;)
;;assert_return
;;  (invoke "param-first-f64" (f64.const 2) (f64.const 3)) (f64.const 2)
;;)
;;assert_return
;;  (invoke "param-second-i32" (i32.const 2) (i32.const 3)) (i32.const 3)
;;)
;;assert_return
;;  (invoke "param-second-i64" (i64.const 2) (i64.const 3)) (i64.const 3)
;;)
;;assert_return
;;  (invoke "param-second-f32" (f32.const 2) (f32.const 3)) (f32.const 3)
;;)
;;assert_return
;;  (invoke "param-second-f64" (f64.const 2) (f64.const 3)) (f64.const 3)
;;)

;;assert_return
;;  (invoke "param-mixed"
;;    (f32.const 1) (i32.const 2) (i64.const 3)
;;    (i32.const 4) (f64.const 5.5) (i32.const 6)
;;  )
;;  (f64.const 5.5)
;;)

;;assert_return (invoke "empty"))
;;assert_return (invoke "value-void"))
;;assert_return (invoke "value-i32") (i32.const 77))
;;assert_return (invoke "value-i64") (i64.const 7777))
;;assert_return (invoke "value-f32") (f32.const 77.7))
;;assert_return (invoke "value-f64") (f64.const 77.77))
;;assert_return (invoke "value-block-void"))
;;assert_return (invoke "value-block-i32") (i32.const 77))

;;assert_return (invoke "return-empty"))
;;assert_return (invoke "return-i32") (i32.const 78))
;;assert_return (invoke "return-i64") (i64.const 7878))
;;assert_return (invoke "return-f32") (f32.const 78.7))
;;assert_return (invoke "return-f64") (f64.const 78.78))
;;assert_return (invoke "return-block-i32") (i32.const 77))

;;assert_return (invoke "break-empty"))
;;assert_return (invoke "break-i32") (i32.const 79))
;;assert_return (invoke "break-i64") (i64.const 7979))
;;assert_return (invoke "break-f32") (f32.const 79.9))
;;assert_return (invoke "break-f64") (f64.const 79.79))
;;assert_return (invoke "break-block-i32") (i32.const 77))

;;assert_return (invoke "break-br_if-empty" (i32.const 0)))
;;assert_return (invoke "break-br_if-empty" (i32.const 2)))
;;assert_return (invoke "break-br_if-num" (i32.const 0)) (i32.const 51))
;;assert_return (invoke "break-br_if-num" (i32.const 1)) (i32.const 50))

;;assert_return (invoke "break-br_table-empty" (i32.const 0)))
;;assert_return (invoke "break-br_table-empty" (i32.const 1)))
;;assert_return (invoke "break-br_table-empty" (i32.const 5)))
;;assert_return (invoke "break-br_table-empty" (i32.const -1)))
;;assert_return (invoke "break-br_table-num" (i32.const 0)) (i32.const 50))
;;assert_return (invoke "break-br_table-num" (i32.const 1)) (i32.const 50))
;;assert_return (invoke "break-br_table-num" (i32.const 10)) (i32.const 50))
;;assert_return (invoke "break-br_table-num" (i32.const -100)) (i32.const 50))
;;assert_return (invoke "break-br_table-nested-empty" (i32.const 0)))
;;assert_return (invoke "break-br_table-nested-empty" (i32.const 1)))
;;assert_return (invoke "break-br_table-nested-empty" (i32.const 3)))
;;assert_return (invoke "break-br_table-nested-empty" (i32.const -2)))
;;assert_return
;;  (invoke "break-br_table-nested-num" (i32.const 0)) (i32.const 52)
;;)
;;assert_return
;;  (invoke "break-br_table-nested-num" (i32.const 1)) (i32.const 50)
;;)
;;assert_return
;;  (invoke "break-br_table-nested-num" (i32.const 2)) (i32.const 52)
;;)
;;assert_return
;;  (invoke "break-br_table-nested-num" (i32.const -3)) (i32.const 52)
;;)

;;assert_return (invoke "init-local-i32") (i32.const 0))
;;assert_return (invoke "init-local-i64") (i64.const 0))
;;assert_return (invoke "init-local-f32") (f32.const 0))
;;assert_return (invoke "init-local-f64") (f64.const 0))
