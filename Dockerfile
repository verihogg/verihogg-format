FROM nixos/nix AS builder

WORKDIR /app

COPY . .

RUN nix-build && \
    nix-store --export $(nix-store -qR result) > /nix-closure.nar

FROM nixos/nix

COPY --from=builder /nix-closure.nar /nix-closure.nar

RUN imported_paths="$(nix-store --import < /nix-closure.nar)" && \
    store_path="$(printf '%s\n' "$imported_paths" | grep 'verihogg-format-' | head -n1)" && \
    test -n "$store_path" && \
    ln -s "$store_path/bin/verihogg-format" /nix/var/nix/profiles/default/bin/ && \
    rm /nix-closure.nar
