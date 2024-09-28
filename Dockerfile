FROM alpine:3.19
RUN apk add python3 bash
COPY .evergreen /src/.evergreen
COPY tools /src/tools
COPY pyproject.toml /src
COPY poetry.lock /src
WORKDIR /src
RUN ./tools/poetry.sh install --with=dev
# TODO: mc-evg-generate fails with:
# """
# pydantic_core._pydantic_core.ValidationError: 2 validation errors for EvgProject
# """
RUN ./tools/poetry.sh run mc-evg-generate 
