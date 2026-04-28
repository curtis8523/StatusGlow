FROM node:20-alpine

WORKDIR /app

COPY package.json ./
COPY server.js ./

ENV STATUSGLOW_CONFIG=/app/config.json
EXPOSE 8787

CMD ["npm", "start"]
