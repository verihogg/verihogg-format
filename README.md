# verihogg-format

> Форматтер кода для SystemVerilog.

**verihogg-format** форматирует SystemVerilog стандарта IEEE 1800-2017, используя парсер slang и переносит строки для оптимальной читаемости.

```
verihogg-format --inplace src/**/*.sv
```

---

## Установка

Для сборки нужен [Nix](https://nixos.org/download/).

```bash
git clone https://github.com/g4rry1/kyrs.git
cd kyrs/g4rry
nix-shell
cmake -B build
cmake --build build
```

Бинарник окажется в `build/bin/verihogg-format`. Добавьте путь в `$PATH` или вызывайте напрямую.

---

## Использование

```bash
# форматировать и вывести в stdout
verihogg-format mymodule.sv

# форматировать несколько файлов на месте
verihogg-format --inplace mymodule.sv
verihogg-format --inplace src/**/*.sv

# прочитать из stdin и вывести в stdout
cat mymodule.sv | verihogg-format
```

### Коды выхода

| Код | Значение                             |
| --- | ------------------------------------ |
| `0` | Успех. Форматирование завершено.     |
| `1` | Ошибка парсинга или обработки файла. |

---

## Параметры командной строки

| Параметр                        | Описание                                                        |
| ------------------------------- | --------------------------------------------------------------- |
| `--inplace`                     | Перезаписать исходные файлы вместо вывода в stdout.             |
| `--column_limit N`              | Максимальная длина строки (дефолт: `100`).                      |
| `--indentation_spaces N`        | Пробелов на уровень отступа (дефолт: `2`).                      |
| `--wrap_spaces N`               | Дополнительный отступ при переносе строки (дефолт: `4`).        |
| `--line_break_penalty N`        | Штраф за перенос строки (дефолт: `2`).                          |
| `--over_column_limit_penalty N` | Штраф за символ сверх лимита (дефолт: `100`).                   |
| `--line_terminator MODE`        | Символ конца строки: `auto` \| `lf` \| `crlf` (дефолт: `auto`). |

### Примеры

```bash
# форматировать с отступом в 4 пробела
verihogg-format --inplace --indentation_spaces 4 mymodule.sv

# установить лимит в 120 символов
verihogg-format --inplace --column_limit 120 mymodule.sv

# форматировать со сложными настройками
verihogg-format --inplace \
  --column_limit 100 \
  --indentation_spaces 2 \
  --wrap_spaces 4 \
  --line_break_penalty 2 \
  --over_column_limit_penalty 100 \
  src/**/*.sv
```

---

## Архитектура

Форматтер построен в виде pipeline из нескольких стадий:

1. **Лексический анализ** — токенизация исходного кода с помощью slang
2. **Аннотирование токенов** — определение типов токенов и контекста
3. **Unwrapping** — развёртывание структуры в логические строки
4. **Выравнивание** — табулярное выравнивание портов и объявлений
5. **Поиск разбиений** — оптимизация переносов строк с использованием штрафов
6. **Печать** — генерация отформатированного кода

---

## Разработка

### Сборка

```bash
nix-shell
cmake -B build
cmake --build build
```

### Тестирование

Проект включает набор тестов на основе Google Test. Для их запуска требуется GTest:

```bash
cmake -B build
cmake --build build
ctest --test-dir build
```

Тесты используют исходные файлы из проекта [SCR1](https://github.com/syntacore/scr1) (RISC-V ядро от Syntacore).

### Структура проекта

```
formatter/
├── include/           # Заголовочные файлы
│   ├── cli/          # Парсинг аргументов командной строки
│   ├── data/         # Структуры данных
│   └── pipeline/     # Стадии форматирования
├── src/              # Исходный код
│   ├── main.cpp      # Точка входа
│   ├── formatter.cpp # Главная логика
│   ├── cli/
│   ├── data/
│   └── pipeline/
└── tests/            # Тесты
```

---

## Пример

Опенсорсный процессор [SCR1](https://github.com/syntacore/scr1) — RISC-V ядро
от Syntacore — форматируется verihogg-format без проблем. Ниже фрагмент
из модуля ALU до и после запуска с дефолтными настройками:

**До**

```verilog
module scr1_pipe_ialu #(parameter SCR1_XLEN = 32)(
input logic clk,input logic rst_n,
input  logic [SCR1_XLEN-1:0]  main_op1,
input logic[SCR1_XLEN-1:0] main_op2,
input  type_scr1_ialu_cmd_e   cmd,
output logic[SCR1_XLEN-1:0] result,
output logic cmp_res
);

always_comb begin
  result='0;
  if(cmd==SCR1_IALU_CMD_AND)
    result=main_op1&main_op2;
  else if(cmd==SCR1_IALU_CMD_OR)
    result=main_op1|main_op2;
end
```

**После**

```verilog
module scr1_pipe_ialu #(parameter SCR1_XLEN = 32) (
input     logic                        clk,
input     logic                        rst_n,
input     logic [SCR1_XLEN - 1 : 0]    main_op1,
input     logic [SCR1_XLEN - 1 : 0]    main_op2,
input     type_scr1_ialu_cmd_e         cmd,
output    logic [SCR1_XLEN - 1 : 0]    result,
output    logic                        cmp_res
);
  always_comb
    begin
      result = '0;
      if (cmd == SCR1_IALU_CMD_AND)
        result = main_op1 & main_op2;
      else if (cmd == SCR1_IALU_CMD_OR)
        result = main_op1 | main_op2;
    end

```

---

## Требования

- C++20 compatible compiler
- slang library (SystemVerilog parser)
- Microsoft GSL (Guidelines Support Library)
- CMake 3.20+
- (опционально) Google Test для тестов

При использовании Nix все зависимости устанавливаются автоматически.

---

## Лицензия

MIT
