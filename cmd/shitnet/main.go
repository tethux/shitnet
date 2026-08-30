package main

import (
	"errors"
	"fmt"
	"io"
	"strings"

	"github.com/chzyer/readline"
)

func main() {
	rl, rlErr := readline.New("shitnet> ")
	if rlErr != nil {
		panic(rlErr)
	}
	defer func() {
		_ = rl.Close()
	}()
	for {
		line, lineErr := rl.Readline()
		if errors.Is(lineErr, readline.ErrInterrupt) {
			continue
		}
		if errors.Is(lineErr, io.EOF) {
			fmt.Println()
			return
		}
		if lineErr != nil {
			panic(lineErr)
		}
		args := strings.Fields(line)
		if len(args) == 0 {
			continue
		}
		switch args[0] {
		case "exit":
			return
		case "help":
			fmt.Println("todo")

		}
	}
}
