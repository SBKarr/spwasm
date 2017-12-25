;; Test all the f64 comparison operators on major boundary values and all
;; special values.

(module
  (func (export "eq") (param $x f64) (param $y f64) (result i32) (f64.eq (get_local $x) (get_local $y)))
  (func (export "ne") (param $x f64) (param $y f64) (result i32) (f64.ne (get_local $x) (get_local $y)))
  (func (export "lt") (param $x f64) (param $y f64) (result i32) (f64.lt (get_local $x) (get_local $y)))
  (func (export "le") (param $x f64) (param $y f64) (result i32) (f64.le (get_local $x) (get_local $y)))
  (func (export "gt") (param $x f64) (param $y f64) (result i32) (f64.gt (get_local $x) (get_local $y)))
  (func (export "ge") (param $x f64) (param $y f64) (result i32) (f64.ge (get_local $x) (get_local $y)))
)
